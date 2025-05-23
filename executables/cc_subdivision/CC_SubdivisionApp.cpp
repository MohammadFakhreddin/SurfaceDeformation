#include "CC_SubdivisionApp.hpp"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/subdivide.h"
#include "Curve.hpp"

#include <omp.h>

#include "Subdivision.hpp"

using namespace geometrycentral::surface;

using namespace MFA;
using namespace shared;

//-----------------------------------------------------
// 1- Implement zoom in an zoom out. (Done)
// 2- implement curtains by projecting and extracting x y z from mesh by a ray from pixel position to the camera forward direction. (Done)
// 3- Draw by right button. (Done)
// 4- Draw on curtain (Done)
// 5- Clear curtain (Done)
// 6- Sample points with a delta (Done)
// 7- Deform mesh using a button (Done)
// 8- Basis function for larger subdivision (Done)
// 9- Fix the remaining bugs, maybe cleanup and write a report.

CC_SubdivisionApp::CC_SubdivisionApp()
{
	MFA_LOG_DEBUG("Loading...");

	omp_set_num_threads(static_cast<int>(static_cast<float>(std::thread::hardware_concurrency()) * 0.8f));
	MFA_LOG_INFO("Number of available workers are: %d", omp_get_max_threads());

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
		sizeof(ColorPipeline::ViewProjection),
		device->GetMaxFramePerFlight()
	);

	camera->Setposition({ 0.0f, -1.0f, 15.0f });

	cameraBufferTracker = std::make_shared<CameraBufferTracker>(
		cameraBuffer,
		ColorPipeline::ViewProjection{
			.viewProjection = camera->GetViewProjection(),
			.cameraPosition = glm::vec4{camera->Getposition(), 1.0f}
		}
	);

	device->ResizeEventSignal2.Register([this]()->void {
		cameraBufferTracker->SetData(
			ColorPipeline::ViewProjection{
				.viewProjection = camera->GetViewProjection(),
				.cameraPosition = glm::vec4{camera->Getposition(), 1.0f}
			}
		);
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
		}
	);

	// Load a surface mesh which is required to be manifold

	std::tie(originalMesh, originalGeometry) = readManifoldSurfaceMesh(Path::Instance->Get("models/cube.obj"));

	std::shared_ptr copyMesh = originalMesh->copy();
	std::shared_ptr copyGeom = originalGeometry->reinterpretTo(*copyMesh);

	surfaceMeshList.emplace_back(std::make_shared<shared::SurfaceMesh>(copyMesh, copyGeom));

	subdivisionDirtyStatus.emplace_back(false);

	meshRenderer = std::make_shared<MeshRenderer>(
		colorPipeline,
		wireFramePipeline,
		surfaceMeshList[subdivisionLevel]
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
		cameraBufferTracker->SetData(ColorPipeline::ViewProjection{
			.viewProjection = camera->GetViewProjection(),
			.cameraPosition = glm::vec4{camera->Getposition(), 1.0f}
		});
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
			.wireFrameModel = glm::scale(glm::identity<glm::mat4>(), {1.01f, 1.01f, 1.01f}),
			.lightColor = lightColor,
			.lightPosition = lightPosition
		}	
	});

	if (drawMode == DrawMode::OnCurtain && drawCurtain == true)
	{
		curtainRenderer->Render(recordState, CurtainRenderer::RenderOptions{
			.useWireframe = false,
			.fillColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
			.wireframeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
			.lightPosition = lightPosition,
			.lightColor = lightColor,
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

		// TODO: Move to a function
		for (int lvl = static_cast<int>(surfaceMeshList.size()) - 1; lvl < subdivisionLevel; ++lvl)
		{
			std::shared_ptr subdividedMesh = surfaceMeshList[lvl]->GetMesh()->copy();
			std::shared_ptr subdividedGeometry = surfaceMeshList[lvl]->GetGeometry()->reinterpretTo(*subdividedMesh);

			std::shared_ptr contribMap = shared::CatmullClarkSubdivide(*subdividedMesh, *subdividedGeometry);

			contributionMapList.emplace_back(contribMap);
			surfaceMeshList.emplace_back(std::make_shared<shared::SurfaceMesh>(subdividedMesh, subdividedGeometry));
			subdivisionDirtyStatus.emplace_back(false);
		}

		for (int lvl = 1; lvl <= subdivisionLevel; ++lvl)
		{
			if (subdivisionDirtyStatus[lvl] == true)
			{
				std::shared_ptr subdividedMesh = surfaceMeshList[lvl - 1]->GetMesh()->copy();
				std::shared_ptr subdividedGeometry = surfaceMeshList[lvl - 1]->GetGeometry()->reinterpretTo(*subdividedMesh);

				geometrycentral::surface::catmullClarkSubdivide(*subdividedMesh, *subdividedGeometry);

				auto const findDeformationsResult = deformationsPerLvl.find(lvl);
				if (findDeformationsResult != deformationsPerLvl.end())
				{
					auto & positions = subdividedGeometry->vertexPositions;
					for (auto & [idx, deformation] : findDeformationsResult->second)
					{
						positions[idx] += deformation;
					}
				}

				surfaceMeshList[lvl]->UpdateGeometry(subdividedMesh, subdividedGeometry);
				subdivisionDirtyStatus[lvl] = false;
			}
		}

		meshRenderer->UpdateGeometry(surfaceMeshList[subdivisionLevel]);
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
	ImGui::InputInt("Laplacian distance", &laplacianDistance);
	ImGui::InputFloat("Laplacian weight", &laplacianWeight);
	ImGui::InputInt("Number of effected levels", &numberOfEffectLevels);
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

			for (int lvl = subdivisionLevel + 1; lvl < static_cast<int>(surfaceMeshList.size()); ++lvl)
			{
				subdivisionDirtyStatus[lvl] = true;
			}
		}
	}
	ImGui::InputFloat4("Light position", reinterpret_cast<float *>(& lightPosition));
	ImGui::InputFloat4("Light color", reinterpret_cast<float*>(&lightColor));
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

	MFA_ASSERT(sampledPoints.size() == projPoints.size());

	std::vector<glm::vec3> movableVertices{};
	std::vector<int> vertexGIndices{};
	std::vector<std::tuple<int, int, float>> vToPContrib{};													// For projection
	CalcVertexToPointContribution(movableVertices, vertexGIndices, vToPContrib);							// Global indices

	std::vector<glm::vec3> allVertices(movableVertices.begin(), movableVertices.end());
	std::vector<std::tuple<int, int, float>> vToVContrib{};													// For laplacian
	std::unordered_map<int, glm::vec3> localIdxToLaplacian{};
	CalcLaplacianContribution(
		movableVertices, 
		vertexGIndices,
		allVertices, 
		vToVContrib, 
		localIdxToLaplacian
	);

	//https://eigen.tuxfamily.org/dox-devel/group__LeastSquares.html
	// I either have to solve it three times or combine them in one giant matrix
	Eigen::MatrixXf B (projPoints.size(), movableVertices.size());
	B.setZero();
	for (auto& [vIdx, pIdx, value] : vToPContrib)
	{
		MFA_ASSERT(B.rows() > pIdx);
		MFA_ASSERT(B.cols() > vIdx);
		B(pIdx, vIdx) += value;
	}
	auto const BT = B.transpose();

	Eigen::MatrixXf Y(allVertices.size(), movableVertices.size());
	Y.setZero();
	for (auto& [nIdx, myIdx, value] : vToVContrib)
	{
		if (Y.rows() > myIdx && Y.cols() > nIdx)
		{
			MFA_ASSERT(Y(myIdx, nIdx) == 0.0f);
			Y(myIdx, nIdx) = value;
		}
	}

	auto const YT = Y.transpose();

	auto const A = (BT * B * (1.0f - laplacianWeight)) + (YT * Y * laplacianWeight);

	Eigen::MatrixXf bx(projPoints.size(), 1);
	Eigen::MatrixXf by(projPoints.size(), 1);
	Eigen::MatrixXf bz(projPoints.size(), 1);
	for (int i = 0; i < static_cast<int>(projPoints.size()); ++i)
	{
		bx(i, 0) = sampledPoints[i].x - projPoints[i].x;
		by(i, 0) = sampledPoints[i].y - projPoints[i].y;
		bz(i, 0) = sampledPoints[i].z - projPoints[i].z;
	}

	Eigen::MatrixXf yX(allVertices.size(), 1);
	Eigen::MatrixXf yY(allVertices.size(), 1);
	Eigen::MatrixXf yZ(allVertices.size(), 1);
	for (int localIdx = 0; localIdx < static_cast<int>(allVertices.size()); ++localIdx)
	{
		auto const& laplacian = localIdxToLaplacian[localIdx];
		yX(localIdx, 0) = -laplacian.x;
		yY(localIdx, 0) = -laplacian.y;
		yZ(localIdx, 0) = -laplacian.z;
	}

	Eigen::BDCSVD<Eigen::MatrixXf> SVD(A, Eigen::ComputeThinU | Eigen::ComputeThinV);

	auto const Dx = SVD.solve((BT * bx * (1.0f - laplacianWeight)) + (YT * yX * laplacianWeight));
	auto const Dy = SVD.solve((BT * by * (1.0f - laplacianWeight)) + (YT * yY * laplacianWeight));
	auto const Dz = SVD.solve((BT * bz * (1.0f - laplacianWeight)) + (YT * yZ * laplacianWeight));

	int lvl = subdivisionLevel - numberOfEffectLevels;

	auto const& subdividedGeometry = surfaceMeshList[lvl]->GetGeometry();
	auto const& subdividedMesh = surfaceMeshList[lvl]->GetMesh();

	if (deformationsPerLvl.contains(lvl) == false)
	{
		deformationsPerLvl[lvl] = {};
	}

	for (int i = 0; i < static_cast<int>(movableVertices.size()); ++i)
	{
		auto const idx = surfaceMeshList[lvl]->GetVertexIdx(movableVertices[i]);

		subdividedGeometry->vertexPositions[idx].x += Dx(i, 0);
		subdividedGeometry->vertexPositions[idx].y += Dy(i, 0);
		subdividedGeometry->vertexPositions[idx].z += Dz(i, 0);

		deformationsPerLvl[lvl].emplace_back(
			std::tuple{
				idx,
				Vector3 {
					Dx(i, 0),
					Dy(i, 0),
					Dz(i, 0)
				}
			}
		);
	}

	surfaceMeshList[lvl]->UpdateGeometry(subdividedMesh, subdividedGeometry);

	for (int nextLvl = lvl + 1; nextLvl <= subdivisionLevel; ++nextLvl)
	{
		int prevLvl = nextLvl - 1;

		std::shared_ptr subdividedMesh = surfaceMeshList[prevLvl]->GetMesh()->copy();
		std::shared_ptr subdividedGeometry = surfaceMeshList[prevLvl]->GetGeometry()->reinterpretTo(*subdividedMesh);

		geometrycentral::surface::catmullClarkSubdivide(*subdividedMesh, *subdividedGeometry);

		auto const findDeformationsResult = deformationsPerLvl.find(nextLvl);
		if (findDeformationsResult != deformationsPerLvl.end())
		{
			auto& positions = subdividedGeometry->vertexPositions;
			for (auto& [idx, deformation] : findDeformationsResult->second)
			{
				positions[idx] += deformation;
			}
		}

		surfaceMeshList[nextLvl]->UpdateGeometry(subdividedMesh, subdividedGeometry);
	}

	meshRenderer->UpdateGeometry(surfaceMeshList[subdivisionLevel]);
	meshCollisionTriangles = meshRenderer->GetCollisionTriangles(meshModelMat);
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
	std::vector<int>& outVertexIndices,
	std::vector<std::tuple<int, int, float>>& outVToPContrib
) const
{
	std::vector<std::tuple<int, int, float>> vToPContrib{};

	std::unordered_map<int, int> vGtoLIdx{}; // Vertex global to local idx
	std::vector<int> lToGIdx{};
	std::vector<int> prevLToGIdx{};

	auto Clear = [&]()->void
	{
		prevLToGIdx = lToGIdx;
		vGtoLIdx.clear();
		lToGIdx.clear();
	};

	auto FindOrInsertVertex = [&](int globalIdx)->int
	{
		auto const findResult = vGtoLIdx.find(globalIdx);
		if (findResult != vGtoLIdx.end())
		{
			return findResult->second;
		}

		int const localIdx = lToGIdx.size();
		vGtoLIdx[globalIdx] = localIdx;
		lToGIdx.emplace_back(globalIdx);

		return localIdx;
	};

	
	for (int pIdx = 0; pIdx < static_cast<int>(projPoints.size()); ++pIdx)
	{
		int const triangleIdx = projTriIndices[pIdx];

		auto const& triangle = meshCollisionTriangles[triangleIdx];

		auto const& v0 = triangle.edgeVertices[0];
		auto const& v1 = triangle.edgeVertices[1];
		auto const& v2 = triangle.edgeVertices[2];

		std::tuple<int, int, int> vIds {};
		auto const foundVertices = surfaceMeshList[subdivisionLevel]->GetVertexIndices(triangleIdx, vIds);
		MFA_ASSERT(foundVertices == true);
		auto& [idx0, idx1, idx2] = vIds;

		auto const coordinate = Math::CalcBarycentricCoordinate(
			projPoints[pIdx],
			v0,
			v1,
			v2
		);

		vToPContrib.emplace_back(std::tuple{ FindOrInsertVertex(idx0), pIdx, coordinate.x });
		vToPContrib.emplace_back(std::tuple{ FindOrInsertVertex(idx1), pIdx, coordinate.y });
		vToPContrib.emplace_back(std::tuple{ FindOrInsertVertex(idx2), pIdx, coordinate.z });
	}

	for (int i = 0; i < numberOfEffectLevels; ++i)
	{
		Clear();

		int currLvlIdx = subdivisionLevel - i;
		int prevLvlIdx = subdivisionLevel - i - 1;

		std::vector<std::tuple<int, int, float>> newVToPGContrib{};
		for (auto const& [lIdx, pIdx, value] : vToPContrib)
		{
			auto const & contribs = contributionMapList[prevLvlIdx]->GetNextLvlContribs(prevLToGIdx[lIdx]);
			for (auto const & contrib : contribs)
			{
				newVToPGContrib.emplace_back(std::tuple{ FindOrInsertVertex(contrib->prevLvlVIdx), pIdx, contrib->amount * value });
			}
		}

		vToPContrib = newVToPGContrib;
	}
	outVToPContrib = vToPContrib;

	auto const lvlVertices = surfaceMeshList[subdivisionLevel - numberOfEffectLevels]->GetVertices();
	outVertexIndices = lToGIdx;
	for (auto gIdx : lToGIdx)
	{
		outVertices.emplace_back(lvlVertices[gIdx].position);
	}
}

//-----------------------------------------------------

void CC_SubdivisionApp::CalcLaplacianContribution(
	std::vector<glm::vec3> & movableVertices,
	std::vector<int> & vertexGIndices,
	std::vector<glm::vec3> & allVertices,													// All vertices must contain movable vertices as well
	std::vector<std::tuple<int, int, float>> & vToVContrib,									// For laplacian
	std::unordered_map<int, glm::vec3> & localIdxToLaplacian
)
{
	std::unordered_map<int, int> gToLVMap{};												// Global to local vertex map
	for (int lIdx = 0; lIdx < static_cast<int>(vertexGIndices.size()); ++lIdx)
	{
		auto wIdx = vertexGIndices[lIdx];
		MFA_ASSERT(gToLVMap.contains(wIdx) == false);
		gToLVMap[wIdx] = lIdx;
	}

	auto const& meshSurface = surfaceMeshList[subdivisionLevel - numberOfEffectLevels];

	// Local index to laplacian
	{
		std::vector<int> queryIndices = vertexGIndices;
		for (int itrCount = 0; itrCount < laplacianDistance; ++itrCount)
		{
			std::vector<int> nextQueryIndices{};
			for (auto myGIdx : queryIndices)
			{
				MFA_ASSERT(gToLVMap.contains(myGIdx));
				auto myLIdx = gToLVMap[myGIdx];

				std::set<int> allVertexNeighbors{};
				{
					bool result = meshSurface->GetVertexNeighbors(myGIdx, allVertexNeighbors);
					MFA_ASSERT(result == true);
				}

				glm::vec3 laplacian = allVertices[myLIdx];
				bool isMovable = itrCount < laplacianDistance - 1;
				bool canInsert = itrCount < laplacianDistance;

				std::vector<int> validNeighbors{};
				for (auto& neighGIdx : allVertexNeighbors)
				{
					auto const findLIdResult = gToLVMap.find(neighGIdx);
					if (findLIdResult == gToLVMap.end())
					{
						if (canInsert == true)
						{
							glm::vec3 position = {};
							{
								bool result = meshSurface->GetVertexPosition(neighGIdx, position);
								MFA_ASSERT(result == true);
							}

							auto const neighLIdx = static_cast<int>(allVertices.size());
							if (isMovable == true)
							{
								movableVertices.emplace_back(position);
							}
							allVertices.emplace_back(position);
							vertexGIndices.emplace_back(neighGIdx);
							nextQueryIndices.emplace_back(neighGIdx);
							gToLVMap[neighGIdx] = neighLIdx;

							validNeighbors.emplace_back(neighGIdx);
						}
					}
					else
					{
						validNeighbors.emplace_back(neighGIdx);
					}
				}

				float weight = 1.0f / static_cast<float>(validNeighbors.size());

				for (auto& neighGIdx : validNeighbors)
				{
					auto const findLIdResult = gToLVMap.find(neighGIdx);

					if (findLIdResult != gToLVMap.end())
					{
						auto const neighLIdx = findLIdResult->second;
						laplacian -= weight * allVertices[neighLIdx];
						vToVContrib.emplace_back(std::tuple{ neighLIdx, myLIdx, -weight });
					}
				}

				vToVContrib.emplace_back(std::tuple{ myLIdx, myLIdx, 1.0f });

				localIdxToLaplacian[myLIdx] = laplacian;
			}
			queryIndices = nextQueryIndices;
		}
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
