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
#include "camera/ArcballCamera.hpp"
#include "camera/ObserverCamera.hpp"
#include "utils/LineRenderer.hpp"
#include "CurtainMeshRenderer.hpp"

#include <memory>

class CC_SubdivisionApp
{
public:

	using Mesh = geometrycentral::surface::ManifoldSurfaceMesh;
	using Geometry = geometrycentral::surface::VertexPositionGeometry;
	using MeshRenderer = shared::SurfaceMeshRenderer;
	using CollisionTriangle = MFA::CollisionTriangle;
	using CurtainRenderer = shared::CurtainMeshRenderer;

    explicit CC_SubdivisionApp();

    ~CC_SubdivisionApp();

    void Run();

private:

    void Update();

	void Render(MFA::RT::CommandRecordState & recordState);

	void OnUI();

	void OnSDL_Event(SDL_Event* event);

	void ClearCurtain();

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

	std::shared_ptr<MFA::LinePipeline> linePipeline{};
	std::shared_ptr<MFA::LineRenderer> lineRenderer{};

	std::shared_ptr<MFA::ColorPipeline> colorPipeline{};
	std::shared_ptr<MFA::ColorPipeline> wireFramePipeline{};

	std::shared_ptr<MFA::ColorPipeline> noCullColorPipeline{};
	std::shared_ptr<MFA::ColorPipeline> noCullWireFramePipeline{};


	std::shared_ptr<Mesh> originalMesh{};
	std::shared_ptr<Geometry> originalGeometry{};

	std::shared_ptr<MeshRenderer> meshRenderer{};
	std::shared_ptr<CurtainRenderer> curtainRenderer{};

	MeshRenderer::RenderOptions meshRendererOptions{
		.useWireframe = true
	};

	std::unique_ptr<MFA::ArcballCamera> camera{};
	// Options
	int subdivisionLevel = 0;
	float curtainHeight = 0.5f;
	float deltaS = 0.1f;

	std::shared_ptr<Mesh> subdividedMesh{};
	std::shared_ptr<Geometry> subdividedGeometry{};

	bool rightMouseDown = false;

	std::vector<glm::vec3> rayCastPoints{};
	std::vector<glm::vec3> rayCastNormals{};

	std::vector<glm::vec3> sampledPoints{};
	std::vector<glm::vec3> sampledNormals{};

	std::vector<CollisionTriangle> meshCollisionTriangles{};
	std::vector<CollisionTriangle> curtainCollisionTriangles{};
	
	enum class DrawMode
	{
		OnMesh,
		OnCurtain
	};
	DrawMode _drawMode = DrawMode::OnMesh;
};
