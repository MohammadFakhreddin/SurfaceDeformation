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
    ui->UpdateSignal.Register([this]()->void{ OnUI(); });

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
        ColorPipeline::Params {
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

	meshCollisionTriangles = meshRenderer->GetCollisionTriangles(glm::identity<glm::mat4>());
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

        int outTriangleIdx = -1;
        glm::dvec3 outTrianglePosition{};
        glm::dvec3 outTriangleNormal{};

        std::vector<CollisionTriangle>* collisionTriangles = nullptr;
        bool checkForBackCollision = false;
    	switch (_drawMode)
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
            outTriangleIdx,
            outTrianglePosition,
            outTriangleNormal,
            checkForBackCollision
		);

        if (hasCollision == true)
        {
            rayCastPoints.emplace_back(outTrianglePosition);
            rayCastNormals.emplace_back(outTriangleNormal);
        }
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

    if (_drawMode == DrawMode::OnCurtain)
    {
        curtainRenderer->Render(recordState, CurtainRenderer::RenderOptions{
            .fillColor = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
            .wireframeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
        });
    }

    {
        for (int i = 0; i < static_cast<int>(rayCastPoints.size()) - 1; ++i)
        {
            lineRenderer->Draw(
                recordState, 
                rayCastPoints[i] + rayCastNormals[i] * 0.01f,
                rayCastPoints[i + 1] + rayCastNormals[i] * 0.01f,
                glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f }
            );
            lineRenderer->Draw(
                recordState,
                rayCastPoints[i] - rayCastNormals[i] * 0.01f,
                rayCastPoints[i + 1] - rayCastNormals[i] * 0.01f,
                glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f }
            );
        }
    }

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
        meshCollisionTriangles = meshRenderer->GetCollisionTriangles(glm::identity<glm::mat4>());
        ClearCurtain();
    }
    if (ImGui::InputFloat("Curtain height", &curtainHeight))
    {
        curtainHeight = std::clamp(curtainHeight, 0.01f, 10.0f);
    	curtainRenderer->UpdateGeometry(sampledPoints, sampledNormals, curtainHeight);
    }
    if (ImGui::InputFloat("Delta s", &deltaS))
    {
        deltaS = std::max(deltaS, 0.01f);
    }
    if (_drawMode == DrawMode::OnCurtain)
    {
        if (ImGui::Button("Clear curtain"))
        {
            ClearCurtain();
        }
        if (ImGui::Button("Deform mesh"))
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
        rayCastPoints.clear();
        rayCastNormals.clear();
    }
    else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_RIGHT)
    {
        rightMouseDown = false;

        if (_drawMode == DrawMode::OnMesh)
        {
            Curve::UniformSample(
                rayCastPoints, 
                rayCastNormals, 
                sampledPoints, 
                sampledNormals, 
                deltaS
            );

            curtainRenderer->UpdateGeometry(sampledPoints, sampledNormals, curtainHeight);
            curtainCollisionTriangles = curtainRenderer->GetCollisionTriangles();

            sampledPoints.clear();
            sampledNormals.clear();

            rayCastPoints.clear();
            rayCastNormals.clear();

        	_drawMode = DrawMode::OnCurtain;
        }
    }
}

//-----------------------------------------------------

void CC_SubdivisionApp::ClearCurtain()
{
    _drawMode = DrawMode::OnMesh;
    rayCastPoints.clear();
    rayCastNormals.clear();
}

//-----------------------------------------------------

void CC_SubdivisionApp::DeformMesh()
{
}

//-----------------------------------------------------
