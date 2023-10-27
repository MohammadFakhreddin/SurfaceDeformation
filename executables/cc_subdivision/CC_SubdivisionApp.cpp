#include "CC_SubdivisionApp.hpp"

#include "SurfaceMeshRenderer.hpp"
#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/subdivide.h"

using namespace geometrycentral::surface;

using namespace MFA;

//-----------------------------------------------------

CC_SubdivisionApp::CC_SubdivisionApp()
{
    MFA_LOG_DEBUG("Loading...");

    path = Path::Instantiate();

    LogicalDevice::InitParams params
    {
        .windowWidth = 800,
        .windowHeight = 600,
        .resizable = true,
        .fullScreen = false,
        .applicationName = "Catmul-Clark subdivison"
    };

    device = LogicalDevice::Instantiate(params);
    assert(device->IsValid() == true);

    camera = std::make_unique<ObserverCamera>();
    
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
        ColorPipeline::Params{}
    );

    wireFramePipeline = std::make_shared<ColorPipeline>(
        displayRenderPass, 
        cameraBuffer, 
        ColorPipeline::Params {.polygonMode = VK_POLYGON_MODE_LINE}
    );

    device->SDL_EventSignal.Register([&](SDL_Event* event)->void
    {
        OnSDL_Event(event);
    });

    // Load a surface mesh which is required to be manifold

    std::tie(mesh, geometry) = readManifoldSurfaceMesh(Path::Instance->Get("models/cube.obj"));
    //catmullClarkSubdivide(*mesh, *geometry);

    meshRenderer = std::make_shared<shared::SurfaceMeshRenderer>(
		colorPipeline,
        wireFramePipeline,
        mesh,
        geometry,
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
    );
}

//-----------------------------------------------------

CC_SubdivisionApp::~CC_SubdivisionApp()
{
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
    ui->EndWindow();
}

//-----------------------------------------------------

void CC_SubdivisionApp::OnSDL_Event(SDL_Event* event)
{
}

//-----------------------------------------------------
