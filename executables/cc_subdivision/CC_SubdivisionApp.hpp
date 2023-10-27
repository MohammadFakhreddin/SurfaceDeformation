#pragma once

#include "BedrockPath.hpp"
#include "BufferTracker.hpp"
#include "LogicalDevice.hpp"
#include "UI.hpp"
#include "pipeline/LinePipeline.hpp"
#include "render_pass/DisplayRenderPass.hpp"
#include "render_resource/DepthRenderResource.hpp"
#include "render_resource/MSAA_RenderResource.hpp"
#include "render_resource/SwapChainRenderResource.hpp"
#include "SurfaceMeshRenderer.hpp"
#include "geometrycentral/surface/manifold_surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"
#include "pipeline/ColorPipeline.hpp"

#include <memory>

#include "camera/ObserverCamera.hpp"

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

	float deltaTimeSec = 0.0f;

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

	std::shared_ptr<MFA::ColorPipeline> colorPipeline{};
	std::shared_ptr<MFA::ColorPipeline> wireFramePipeline{};

	std::shared_ptr<geometrycentral::surface::ManifoldSurfaceMesh> mesh{};
	std::shared_ptr<geometrycentral::surface::VertexPositionGeometry> geometry{};

	std::shared_ptr<shared::SurfaceMeshRenderer> meshRenderer{};
	shared::SurfaceMeshRenderer::RenderOptions meshRendererOptions{
		.useWireframe = true
	};

	std::unique_ptr<MFA::ObserverCamera> camera{};
	

};
