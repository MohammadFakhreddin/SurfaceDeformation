#include "BSplineApp.hpp"

using namespace MFA;

//-----------------------------------------------------

BSplineApp::BSplineApp()
{
        MFA_LOG_DEBUG("Loading...");

    path = Path::Instantiate();

    LogicalDevice::InitParams params
    {
        .windowWidth = 800,
        .windowHeight = 600,
        .resizable = true,
        .fullScreen = false,
        .applicationName = "BSpline"
    };

    device = LogicalDevice::Instantiate(params);
    assert(device->IsValid() == true);

    
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

    cameraBufferTracker = std::make_shared<HostVisibleBufferTracker<glm::mat4>>(cameraBuffer, glm::identity<glm::mat4>());

    device->ResizeEventSignal2.Register([this]()->void {
        cameraBufferTracker->SetData(glm::identity<glm::mat4>());
    });

    linePipeline = std::make_shared<LinePipeline>(displayRenderPass, cameraBuffer, 10000);
    pointPipeline = std::make_shared<PointPipeline>(displayRenderPass, cameraBuffer, 10000);

    device->SDL_EventSignal.Register([&](SDL_Event* event)->void
    {
        OnSDL_Event(event);
    });
}

//-----------------------------------------------------

BSplineApp::~BSplineApp()
{
    linePipeline.reset();
    pointPipeline.reset();
    cameraBufferTracker.reset();
	cameraBuffer.reset();
    ui.reset();
    displayRenderPass.reset();
    swapChainResource.reset();
    depthResource.reset();
    msaaResource.reset();
    device.reset();
    path.reset();
}

//-----------------------------------------------------

void BSplineApp::Run()
{
    SDL_GL_SetSwapInterval(0);
    SDL_Event e;
    uint32_t deltaTimeMs = 1000.0f / 30.0f;
    float deltaTimeSec = static_cast<float>(deltaTimeMs) / 1000.0f;
    uint32_t startTime = SDL_GetTicks();

    bool shouldQuit = false;

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

        device->Update();

        ui->Update();

        Update();

        auto recordState = device->AcquireRecordState(swapChainResource->GetSwapChainImages().swapChain);
        if (recordState.isValid == true)
        {
            device->BeginCommandBuffer(
                recordState,
                RT::CommandBufferType::Graphic
            );

            cameraBufferTracker->Update(recordState);

            displayRenderPass->Begin(recordState);

            Render(recordState);

        	ui->Render(recordState, deltaTimeSec);

            displayRenderPass->End(recordState);

            device->EndCommandBuffer(recordState);

            device->SubmitQueues(recordState);

            device->Present(recordState, swapChainResource->GetSwapChainImages().swapChain);
        }

        deltaTimeMs = std::max<uint32_t>(SDL_GetTicks() - startTime, static_cast<uint32_t>(1000.0f / 240.0f));
        startTime = SDL_GetTicks();
        deltaTimeSec = static_cast<float>(deltaTimeMs) / 1000.0f;
    }

    device->DeviceWaitIdle();
}

//-----------------------------------------------------

void BSplineApp::Update()
{

}

//-----------------------------------------------------

void BSplineApp::Render(MFA::RT::CommandRecordState& recordState)
{

}

//-----------------------------------------------------

void BSplineApp::OnUI()
{
    ui->BeginWindow("Settings");
    ui->EndWindow();
}

//-----------------------------------------------------

void BSplineApp::OnSDL_Event(SDL_Event* event)
{
}

//-----------------------------------------------------
