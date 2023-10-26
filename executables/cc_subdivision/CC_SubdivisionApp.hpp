#pragma once

#include "BedrockPath.hpp"
#include "BufferTracker.hpp"
#include "LogicalDevice.hpp"
#include "UI.hpp"
#include "camera/PerspectiveCamera.hpp"
#include "pipeline/LinePipeline.hpp"
#include "pipeline/PointPipeline.hpp"
#include "render_pass/DisplayRenderPass.hpp"
#include "render_resource/DepthRenderResource.hpp"
#include "render_resource/MSAA_RenderResource.hpp"
#include "render_resource/SwapChainRenderResource.hpp"

#include <memory>

class CC_SubdivisionApp
{
public:

    explicit CC_SubdivisionApp();

    ~CC_SubdivisionApp();

    void Run();

private:

    void Update();

	void Render(MFA::RT::CommandRecordState & recordState);

	void OnUI();

	void OnSDL_Event(SDL_Event* event);

	// Render parameters
	std::shared_ptr<MFA::Path> path{};
	std::shared_ptr<MFA::LogicalDevice> device{};
	std::shared_ptr<MFA::UI> ui{};
	std::shared_ptr<MFA::SwapChainRenderResource> swapChainResource{};
	std::shared_ptr<MFA::DepthRenderResource> depthResource{};
	std::shared_ptr<MFA::MSSAA_RenderResource> msaaResource{};
	std::shared_ptr<MFA::DisplayRenderPass> displayRenderPass{};

	std::shared_ptr<MFA::RT::BufferGroup> cameraBuffer{};
	std::shared_ptr<MFA::HostVisibleBufferTracker<glm::mat4>> cameraBufferTracker{};

	std::shared_ptr<MFA::LinePipeline> linePipeline{};
	std::shared_ptr<MFA::PointPipeline> pointPipeline{};

};