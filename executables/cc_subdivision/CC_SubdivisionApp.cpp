#include "CC_SubdivisionApp.hpp"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/subdivide.h"
#include "Curve.hpp"

using namespace geometrycentral::surface;

using namespace MFA;
using namespace shared;

//-----------------------------------------------------
// 1- Implement zoom in an zoom out. (Done)
// 2- implement curtains by projecting and extracting x y z from mesh by a ray from pixel position to the camera forward direction. (Done)
// 3- Draw by right button. (Done)
// 4- Draw on curtain (Done)
// 5- Clear curtain (Done)
// 6- Sample points with a delta
// 7- Deform mesh using a button
// 8- Basis function for larger subdivision

CC_SubdivisionApp::CC_SubdivisionApp()
{
	MFA_LOG_DEBUG("Loading...");

	path = Path::Instantiate();

	LogicalDevice::InitParams params
	{
		.windowWidth = 1920,
		.windowHeight = 1080,
		.resizable = true,
		.fullScreen = false,
		.applicationName = "Catmul-Clark subdivison"
	};

	device = LogicalDevice::Instantiate(params);
	assert(device->IsValid() == true);

	camera = std::make_unique<ArcballCamera>();

	swapChainResource = std::make_shared<SwapChainRenderResource>();
	depthResource = std::make_shared<DepthRenderResource>();
	msaaResource = std::make_shared<MSSAA_RenderResource>();
	displayRenderPass = std::make_shared<DisplayRenderPass>(
		swapChainResource,
		depthResource,
		msaaResource
	);

	ui = std::make_shared<UI>(displayRenderPass);
	ui->UpdateSignal.Register([this]()->void { OnUI(); });

	cameraBuffer = RB::CreateHostVisibleUniformBuffer(
		device->GetVkDevice(),
		device->GetPhysicalDevice(),
		sizeof(glm::mat4),
		device->GetMaxFramePerFlight()
	);

	camera->Setposition({ 0.0f, -1.0f, 15.0f });

	cameraBufferTracker = std::make_shared<HostVisibleBufferTracker<glm::mat4>>(cameraBuffer, camera->GetViewProjection());

	device->ResizeEventSignal2.Register([this]()->void {
		cameraBufferTracker->SetData(camera->GetViewProjection());
		});

	colorPipeline = std::make_shared<ColorPipeline>(
		displayRenderPass,
		cameraBuffer,
		ColorPipeline::Params{
			.cullModeFlags = VK_CULL_MODE_BACK_BIT,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		}
	);

	wireFramePipeline = std::make_shared<ColorPipeline>(
		displayRenderPass,
		cameraBuffer,
		ColorPipeline::Params{
			.cullModeFlags = VK_CULL_MODE_BACK_BIT,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.polygonMode = VK_POLYGON_MODE_LINE,
		}
	);

	noCullColorPipeline = std::make_shared<ColorPipeline>(
		displayRenderPass,
		cameraBuffer,
		ColorPipeline::Params{
			.cullModeFlags = VK_CULL_MODE_NONE,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		}
	);

	noCullWireFramePipeline = std::make_shared<ColorPipeline>(
		displayRenderPass,
		cameraBuffer,
		ColorPipeline::Params{
			.cullModeFlags = VK_CULL_MODE_NONE,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.polygonMode = VK_POLYGON_MODE_LINE,
		}
	);


	device->SDL_EventSignal.Register([&](SDL_Event* event)->void
		{
			OnSDL_Event(event);
		});

	// Load a surface mesh which is required to be manifold

	std::tie(originalMesh, originalGeometry) = readManifoldSurfaceMesh(Path::Instance->Get("models/cube.obj"));

	subdividedMesh = originalMesh->copy();
	subdividedGeometry = originalGeometry->copy();

	meshRenderer = std::make_shared<MeshRenderer>(
		colorPipeline,
		wireFramePipeline,
		subdividedMesh,
		subdividedGeometry
	);
	curtainRenderer = std::make_shared<CurtainRenderer>(
		noCullColorPipeline,
		noCullWireFramePipeline,
		std::vector<glm::vec3>{},
		std::vector<glm::vec3>{},
		curtainHeight
	);

	meshCollisionTriangles = meshRenderer->GetCollisionTriangles(meshModelMat);
	curtainCollisionTriangles = curtainRenderer->GetCollisionTriangles();

	linePipeline = std::make_shared<LinePipeline>(displayRenderPass, cameraBuffer, 10000);
	lineRenderer = std::make_shared<LineRenderer>(linePipeline);
}

//-----------------------------------------------------

CC_SubdivisionApp::~CC_SubdivisionApp()
{
	lineRenderer.reset();
	linePipeline.reset();
	curtainRenderer.reset();
	meshRenderer.reset();
	noCullColorPipeline.reset();
	noCullWireFramePipeline.reset();
	colorPipeline.reset();
	wireFramePipeline.reset();
	cameraBufferTracker.reset();
	cameraBuffer.reset();
	camera.reset();
	ui.reset();
	displayRenderPass.reset();
	swapChainResource.reset();
	depthResource.reset();
	msaaResource.reset();
	device.reset();
	path.reset();
}

//-----------------------------------------------------

void CC_SubdivisionApp::Run()
{
	SDL_GL_SetSwapInterval(0);
	SDL_Event e;
	uint32_t deltaTimeMs = 1000.0f / 30.0f;
	deltaTimeSec = static_cast<float>(deltaTimeMs) / 1000.0f;
	uint32_t startTime = SDL_GetTicks();

	bool shouldQuit = false;

	uint32_t minDeltaTimeMs = static_cast<uint32_t>(1000.0f / 120.0f);

	while (shouldQuit == false)
	{
		//Handle events
		while (SDL_PollEvent(&e) != 0)
		{
			//User requests quit
			if (e.type == SDL_QUIT)
			{
				shouldQuit = true;
			}
		}

		Update();

		auto recordState = device->AcquireRecordState(swapChainResource->GetSwapChainImages().swapChain);
		if (recordState.isValid == true)
		{
			Render(recordState);
		}

		deltaTimeMs = SDL_GetTicks() - startTime;
		if (deltaTimeMs < minDeltaTimeMs)
		{
			SDL_Delay(minDeltaTimeMs - deltaTimeMs);
			deltaTimeMs = minDeltaTimeMs;
		}
		deltaTimeSec = static_cast<float>(deltaTimeMs) / 1000.0f;
		startTime = SDL_GetTicks();

	}

	device->DeviceWaitIdle();
}

//-----------------------------------------------------

void CC_SubdivisionApp::Update()
{
	device->Update();
	ui->Update();
	camera->Update(deltaTimeSec);
	if (camera->IsDirty())
	{
		cameraBufferTracker->SetData(camera->GetViewProjection());
	}

	if (rightMouseDown == true)
	{
		PerformRaycast();

	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::Render(MFA::RT::CommandRecordState& recordState)
{
	device->BeginCommandBuffer(
		recordState,
		RT::CommandBufferType::Graphic
	);

	cameraBufferTracker->Update(recordState);

	displayRenderPass->Begin(recordState);

	meshRenderer->Render(recordState, meshRendererOptions, std::vector{
		MeshRenderer::InstanceOptions {
			.fillColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
			.wireFrameColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
			.fillModel = glm::identity<glm::mat4>(),
			.wireFrameModel = glm::scale(glm::identity<glm::mat4>(), {1.01f, 1.01f, 1.01f})
		}
		});

	if (drawMode == DrawMode::OnCurtain && drawCurtain == true)
	{
		curtainRenderer->Render(recordState, CurtainRenderer::RenderOptions{
			.fillColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
			.wireframeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
			});
	}

	DrawPoints(
		recordState,
		rayCastPoints,
		rayCastNormals,
		glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f }
	);

	DrawPoints(
		recordState,
		std::vector<glm::vec3>(projPoints.begin(), projPoints.end()),
		std::vector<glm::vec3>(projNormals.begin(), projNormals.end()),
		glm::vec4{ 1.0f, 1.0f, 0.0f, 1.0f }
	);

	ui->Render(recordState, deltaTimeSec);

	displayRenderPass->End(recordState);

	device->EndCommandBuffer(recordState);

	device->SubmitQueues(recordState);

	device->Present(recordState, swapChainResource->GetSwapChainImages().swapChain);
}

//-----------------------------------------------------

void CC_SubdivisionApp::OnUI()
{
	ui->BeginWindow("Settings");
	ImGui::Text("Delta time: %f", deltaTimeSec);
	if (ImGui::InputInt("Subdivision level", &subdivisionLevel))
	{
		if (subdivisionLevel < 0)
		{
			subdivisionLevel = 0;
		}

		subdividedMesh = originalMesh->copy();
		subdividedGeometry = originalGeometry->reinterpretTo(*subdividedMesh);

		for (int i = 0; i < subdivisionLevel; ++i)
		{
			catmullClarkSubdivide(*subdividedMesh, *subdividedGeometry);
		}

		meshRenderer->UpdateGeometry(subdividedMesh, subdividedGeometry);
		meshCollisionTriangles = meshRenderer->GetCollisionTriangles(meshModelMat);

		ClearCurtain();
		ClearRaycastPoints();
		ClearPorjectedPoints();
	}
	if (ImGui::InputFloat("Curtain height", &curtainHeight))
	{
		curtainHeight = std::clamp(curtainHeight, 0.01f, 10.0f);
		curtainRenderer->UpdateGeometry(curtainHeight);
		curtainCollisionTriangles = curtainRenderer->GetCollisionTriangles();
	}
	if (ImGui::InputFloat("Delta s", &deltaS))
	{
		deltaS = std::max(deltaS, 0.001f);
	}
	ImGui::Checkbox("Curtain", &drawCurtain);
	if (drawMode == DrawMode::OnCurtain)
	{
		if (ImGui::Button("Clear curtain"))
		{
			ClearCurtain();
		}
		if (rayCastPoints.size() >= 2 && ImGui::Button("Deform mesh"))
		{
			DeformMesh();
		}
	}
	ui->EndWindow();
}

//-----------------------------------------------------

void CC_SubdivisionApp::OnSDL_Event(SDL_Event* event)
{
	if (ui->HasFocus() == true)
	{
		return;
	}

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT)
	{
		rightMouseDown = true;
		ClearRaycastPoints();
	}
	else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_RIGHT)
	{
		rightMouseDown = false;

		if (drawMode == DrawMode::OnMesh)
		{
			ClearSamplePoints();

			Curve::UniformSample(
				rayCastPoints,
				rayCastNormals,
				sampledPoints,
				sampledNormals,
				deltaS
			);

			if (sampledPoints.size() >= 2)
			{
				curtainRenderer->UpdateGeometry(sampledPoints, sampledNormals, curtainHeight);
				curtainCollisionTriangles = curtainRenderer->GetCollisionTriangles();
				drawMode = DrawMode::OnCurtain;
			}
			ClearRaycastPoints();
		}
	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::ClearCurtain()
{
	drawMode = DrawMode::OnMesh;
	ClearRaycastPoints();
}

//-----------------------------------------------------

void CC_SubdivisionApp::DeformMesh()
{
	ProjectCurtainPoints();
	ClearCurtain();

	std::vector<glm::vec3> vertices{};
	std::vector<std::tuple<int, int, float>> vToPContrib{};
	CalcVertexToPointContribution(vertices, vToPContrib);

	//https://eigen.tuxfamily.org/dox-devel/group__LeastSquares.html
	// I either have to solve it three times or combine them in one giant matrix
	Eigen::MatrixXf B(projPoints.size(), vertices.size());
	B.setZero();

	for (auto& [vIdx, pIdx, value] : vToPContrib)
	{
		MFA_ASSERT(B(pIdx, vIdx) == 0.0f);
		B(pIdx, vIdx) = value;
	}

	auto const BT = B.transpose();
	auto const A = BT * B;

	Eigen::MatrixXf bx(projPoints.size(), 1);
	Eigen::MatrixXf by(projPoints.size(), 1);
	Eigen::MatrixXf bz(projPoints.size(), 1);
	for (int i = 0; i < static_cast<int>(projPoints.size()); ++i)
	{
		bx(i, 0) = sampledPoints[i].x - projPoints[i].x;
		by(i, 0) = sampledPoints[i].y - projPoints[i].y;
		bz(i, 0) = sampledPoints[i].z - projPoints[i].z;
	}

	Eigen::BDCSVD<Eigen::MatrixXf> SVD(A, Eigen::ComputeThinU | Eigen::ComputeThinV);

	auto const Dx = SVD.solve(BT * bx);
	auto const Dy = SVD.solve(BT * by);
	auto const Dz = SVD.solve(BT * bz);

	auto const& vertexPositions = subdividedGeometry->vertexPositions;

	auto const FindVertexIdx = [&vertexPositions](glm::vec3 const & position)->int
	{
		for (int i = 0; i < static_cast<int>(vertexPositions.size()); ++i)
		{
			auto const& vPos = vertexPositions[i];
			auto const distance2 = std::pow(vPos.x - position.x, 2) + 
				std::pow(vPos.y - position.y, 2) + 
				std::pow(vPos.z - position.z, 2);

			if (distance2 < glm::epsilon<float>())
			{
				return i;
			}
		}
		return -1;
	};

	for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
	{
		auto const idx = FindVertexIdx(vertices[i]);
		subdividedGeometry->vertexPositions[idx].x += Dx(i, 0);
		subdividedGeometry->vertexPositions[idx].y += Dy(i, 0);
		subdividedGeometry->vertexPositions[idx].z += Dz(i, 0);
	}

	meshRenderer->UpdateGeometry(subdividedMesh, subdividedGeometry);
}

//-----------------------------------------------------

void CC_SubdivisionApp::DrawPoints(
	MFA::RT::CommandRecordState& recordState,
	std::vector<glm::vec3> const& points,
	std::vector<glm::vec3> const& normals,
	glm::vec4 const& color
) const
{
	for (int i = 0; i < static_cast<int>(points.size()) - 1; ++i)
	{
		lineRenderer->Draw(
			recordState,
			points[i] + normals[i] * 0.01f,
			points[i + 1] + normals[i] * 0.01f,
			color
		);
		lineRenderer->Draw(
			recordState,
			points[i] - normals[i] * 0.01f,
			points[i + 1] - normals[i] * 0.01f,
			color
		);
	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::PerformRaycast()
{
	int mx, my;
	SDL_GetMouseState(&mx, &my);

	auto const surfaceCapabilities = LogicalDevice::Instance->GetSurfaceCapabilities();
	// TODO: Move this to Math. It can be very useful
	auto const projMousePos = Math::ScreenSpaceToProjectedSpace(
		glm::vec2{ mx, my },
		static_cast<float>(surfaceCapabilities.currentExtent.width),
		static_cast<float>(surfaceCapabilities.currentExtent.height)
	);

	auto const viewMousePos =
		glm::inverse(camera->GetProjection()) *
		glm::vec4{ projMousePos.x, projMousePos.y, -1.0f, 1.0f };

	glm::vec3 const worldMousePos = glm::inverse(camera->GetView()) *
		glm::vec4{ viewMousePos.x, viewMousePos.y, viewMousePos.z, 1.0f };

	// TODO: Move this to Math. It can be very useful
	auto const cameraDirection = glm::normalize(worldMousePos - camera->Getposition());

	int triangleIdx = -1;
	glm::dvec3 trianglePosition{};
	glm::dvec3 triangleNormal{};

	std::vector<CollisionTriangle>* collisionTriangles = nullptr;
	bool checkForBackCollision = false;
	switch (drawMode)
	{
	case DrawMode::OnCurtain:
	{
		collisionTriangles = &curtainCollisionTriangles;
		checkForBackCollision = true;
	}
	break;
	case DrawMode::OnMesh:
	{
		collisionTriangles = &meshCollisionTriangles;
		checkForBackCollision = false;
	}
	break;
	}

	auto const hasCollision = Collision::HasContiniousCollision(
		*collisionTriangles,
		worldMousePos,
		worldMousePos + (cameraDirection * 1000.0f),
		triangleIdx,
		trianglePosition,
		triangleNormal,
		checkForBackCollision
	);

	if (hasCollision == true)
	{
		rayCastPoints.emplace_back(trianglePosition);
		rayCastNormals.emplace_back(triangleNormal);
		rayCastTriIndices.emplace_back(triangleIdx);
	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::ProjectCurtainPoints()
{
	std::vector<glm::vec3> projDirections{};
	for (auto const rayCastTriIndex : rayCastTriIndices)
	{
		projDirections.emplace_back(curtainRenderer->GetTriangleProjectionDirection(rayCastTriIndex));
	}

	ClearSamplePoints();

	std::vector<glm::vec3> allSampledPoints{};
	std::vector<glm::vec3> allSampledNormals{};

	Curve::UniformSample(
		rayCastPoints,
		projDirections,
		allSampledPoints,
		allSampledNormals,
		deltaS
	);

	ClearPorjectedPoints();

	for (int i = 0; i < static_cast<int>(allSampledPoints.size()); ++i)
	{
		auto const prevPoint = allSampledPoints[i];
		auto const nextPoint = allSampledPoints[i] + (allSampledNormals[i] * 1000.0f);

		int triIdx{};
		glm::dvec3 colPosition{};
		glm::dvec3 colNormal{};

		auto const hasCollision = Collision::HasContiniousCollision(
			meshCollisionTriangles,
			prevPoint,
			nextPoint,
			triIdx,
			colPosition,
			colNormal,
			false
		);

		if (hasCollision == true)
		{
			projPoints.emplace_back(colPosition);
			projNormals.emplace_back(colNormal);
			projTriIndices.emplace_back(triIdx);

			sampledPoints.emplace_back(allSampledPoints[i]);
			sampledNormals.emplace_back(allSampledNormals[i]);
		}
	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::CalcVertexToPointContribution(
	std::vector<glm::vec3>& outVertices,
	std::vector<std::tuple<int, int, float>>& outVToPContrib
) const
{
	outVertices.clear();
	outVToPContrib.clear();

	auto FindVertexIdx = [&outVertices](glm::vec3 position)->int
		{
			int i = 0;
			for (; i < static_cast<int>(outVertices.size()); ++i)
			{
				if (glm::length2(outVertices[i] - position) < glm::epsilon<float>())
				{
					return i;
				}
			}
			outVertices.emplace_back(position);
			return i;
		};

	for (int i = 0; i < static_cast<int>(projPoints.size()); ++i)
	{
		auto const& triangle = meshCollisionTriangles[projTriIndices[i]];
		auto const& v0 = triangle.edgeVertices[0];
		auto const& v1 = triangle.edgeVertices[1];
		auto const& v2 = triangle.edgeVertices[2];

		auto const coordinate = Math::CalcBarycentricCoordinate(
			projPoints[i],
			v0,
			v1,
			v2
		);

		outVToPContrib.emplace_back(std::tuple{ FindVertexIdx(v0), i, coordinate.x });
		outVToPContrib.emplace_back(std::tuple{ FindVertexIdx(v1), i, coordinate.y });
		outVToPContrib.emplace_back(std::tuple{ FindVertexIdx(v2), i, coordinate.z });
	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::ClearRaycastPoints()
{
	rayCastPoints.clear();
	rayCastNormals.clear();
	rayCastTriIndices.clear();
}

//-----------------------------------------------------

void CC_SubdivisionApp::ClearPorjectedPoints()
{
	projPoints.clear();
	projNormals.clear();
	projTriIndices.clear();
}

//-----------------------------------------------------

void CC_SubdivisionApp::ClearSamplePoints()
{
	sampledPoints.clear();
	sampledNormals.clear();
}

//-----------------------------------------------------
