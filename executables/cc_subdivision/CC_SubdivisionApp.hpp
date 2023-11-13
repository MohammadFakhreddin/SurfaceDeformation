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

#include "Contribution.hpp"

class CC_SubdivisionApp
{
public:

	using Mesh = geometrycentral::surface::ManifoldSurfaceMesh;
	using Geometry = geometrycentral::surface::VertexPositionGeometry;
	using MeshRenderer = shared::SurfaceMeshRenderer;
	using CollisionTriangle = MFA::CollisionTriangle;
	using CurtainRenderer = shared::CurtainMeshRenderer;
	using CameraBufferTracker = MFA::HostVisibleBufferTracker<MFA::ColorPipeline::ViewProjection>;

	explicit CC_SubdivisionApp();

	~CC_SubdivisionApp();

	void Run();

private:

	void Update();

	void Render(MFA::RT::CommandRecordState& recordState);

	void OnUI();

	void OnSDL_Event(SDL_Event* event);

	void ClearCurtain();

	void DeformMesh();

	void DrawPoints(
		MFA::RT::CommandRecordState& recordState,
		std::vector<glm::vec3> const& points,
		std::vector<glm::vec3> const& normals,
		glm::vec4 const& color
	) const;

	void PerformRaycast();

	void ProjectCurtainPoints();

	void CalcVertexToPointContribution(
		std::vector<glm::vec3>& outVertices,
		std::vector<int>& outVertexIndices,
		std::vector<std::tuple<int, int, float>>& outVToPContrib
	) const;

	void ClearRaycastPoints();

	void ClearPorjectedPoints();

	void ClearSamplePoints();

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
	std::shared_ptr<CameraBufferTracker> cameraBufferTracker{};

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

	std::unique_ptr<MFA::PerspectiveCamera> camera{};
	// Options
	int subdivisionLevel = 0;
	float curtainHeight = 0.5f;
	float deltaS = 0.001f;

	std::vector<std::shared_ptr<shared::ContributionMap>> contributionMapList{};
	std::vector<std::shared_ptr<Mesh>> subdividedMeshList{};
	std::vector<std::shared_ptr<Geometry>> subdividedGeometryList{};
	std::vector<bool> subdivisionDirtyStatus{};
	std::unordered_map<int, std::vector<std::tuple<int, geometrycentral::Vector3>>> deformationsPerLvl{};

	struct Contribution
	{
		int nextLvlVIdx = -1;			// Next level vertex idx
		int prevLvlVIdx = -1;			// Prev level vertex idx
		float amount = 0.0f;			// Contribution amount
	};

	struct ContributionsMap
	{
		int prevLevel = -1;
		int nextLevel = -1;
		std::vector<Contribution> contributions{};
		std::unordered_map<int, int> prevLvlContribIdx{};
		std::unordered_map<int, int> nextLvlContribIdx{};
	};

	std::vector<ContributionsMap> contributionList{};

	bool rightMouseDown = false;
	// This points are stored globally for better debugging and possible memory reuse.
	std::vector<glm::vec3> rayCastPoints{};
	std::vector<glm::vec3> rayCastNormals{};
	std::vector<int> rayCastTriIndices{};
	
	std::vector<glm::dvec3> projPoints{};
	std::vector<glm::dvec3> projNormals{};
	std::vector<int> projTriIndices{};

	std::vector<glm::vec3> sampledPoints{};
	std::vector<glm::vec3> sampledNormals{};

	std::vector<CollisionTriangle> meshCollisionTriangles{};
	std::vector<CollisionTriangle> curtainCollisionTriangles{};
	
	enum class DrawMode
{
		OnMesh,
		OnCurtain
	};
	DrawMode drawMode = DrawMode::OnMesh;

	bool drawCurtain = true;

	glm::mat4 meshModelMat = glm::identity<glm::mat4>();

	glm::vec4 lightColor = glm::vec4(2.0f, 2.0f, 2.0f, 1.0f);
	glm::vec4 lightPosition = glm::vec4(100.0f, -100.0f, 0.0f, 100.0f);

	int laplacianDistance = 5;
	float laplacianWeight = 0.9f;
};
