#include "CC_SubdivisionApp.hpp"

#include "SurfaceMeshRenderer.hpp"
#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/subdivide.h"

using namespace geometrycentral::surface;

using namespace MFA;

//-----------------------------------------------------
// TODO:
// 1- Implement zoom in an zoom out.
// 2- implement curtains by projecting and extracting x y z from mesh by a ray from pixel position to the camera forward direction.
// 3- Draw by right button.

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

    device->SDL_EventSignal.Register([&](SDL_Event* event)->void
    {
        OnSDL_Event(event);
    });

    // Load a surface mesh which is required to be manifold

    std::tie(originalMesh, originalGeometry) = readManifoldSurfaceMesh(Path::Instance->Get("models/cube.obj"));
    
    subdividedMesh = originalMesh->copy();
    subdividedGeometry = originalGeometry->copy();

    meshRenderer = std::make_shared<shared::SurfaceMeshRenderer>(
		colorPipeline,
        wireFramePipeline,
        subdividedMesh,
        subdividedGeometry,
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    linePipeline = std::make_shared<LinePipeline>(displayRenderPass, cameraBuffer, 10000);
    lineRenderer = std::make_shared<LineRenderer>(linePipeline);
}

//-----------------------------------------------------

CC_SubdivisionApp::~CC_SubdivisionApp()
{
    lineRenderer.reset();
    linePipeline.reset();
    meshRenderer.reset();
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

    if (_rightMouseDown == true)
    {
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        auto const surfaceCapabilities = LogicalDevice::Instance->GetSurfaceCapabilities();

        auto const projMousePos = Math::ScreenSpaceToProjectedSpace(
            glm::vec2{ mx, my },
            static_cast<float>(surfaceCapabilities.currentExtent.width),
            static_cast<float>(surfaceCapabilities.currentExtent.height)
        );

        auto const viewMousePos =
            glm::inverse(camera->GetProjection()) *
            glm::vec4{ projMousePos.x, projMousePos.y, -1.0f, 1.0f };

        auto const worldMousePos = glm::inverse(camera->GetView()) *
            glm::vec4{ viewMousePos.x, viewMousePos.y, viewMousePos.z, 1.0f };

        auto const& cameraDirection = camera->GetForward();

        rayPoints.emplace_back(glm::vec3{ worldMousePos });
        rayDirs.emplace_back(glm::normalize(cameraDirection));
        rayRemLife.emplace_back(1.0f);
        // TODO: Project to mesh
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

    meshRenderer->Render(recordState, meshRendererOptions);

    for (int i = 0; i < static_cast<int>(rayPoints.size()) - 1; ++i)
    {
        lineRenderer->Draw(recordState, rayPoints[i], rayPoints[i + 1], glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f });
    }
    for (int i = 0; i < static_cast<int>(rayPoints.size()); ++i)
    {
        rayPoints[i] += rayDirs[i] * 0.01f;
        rayRemLife[i] -= 0.01f;
    }
    for (int i = static_cast<int>(rayPoints.size()) - 1; i >= 0; --i)
    {
        if (rayRemLife[i] < 0.0f)
        {
            rayPoints.erase(rayPoints.begin() + i);
            rayRemLife.erase(rayRemLife.begin() + i);
            rayDirs.erase(rayDirs.begin() + i);
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
        _rightMouseDown = true;
        
    }
    else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_RIGHT)
    {
        _rightMouseDown = false;
    }
}

//-----------------------------------------------------
