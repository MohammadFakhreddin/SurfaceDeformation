#pragma once
#include <unordered_map>

#include "Collision.hpp"
#include "pipeline/ColorPipeline.hpp"

namespace shared
{
	// TODO: I'm not sure but I need a arc length parametrization from curtain points and sampled points. I think. Maybe I have to use a curve
	class CurtainMeshRenderer
	{
	public:

		using Pipeline = MFA::ColorPipeline;
		using RecordState = MFA::RT::CommandRecordState;
		using CollisionTriangle = MFA::CollisionTriangle;
		using Index = uint32_t;
		// Because this an mesh with zero depth, back-face culling should be disabled
		explicit CurtainMeshRenderer(
			std::shared_ptr<Pipeline> colorPipeline,
			std::shared_ptr<Pipeline> wireFramePipeline,
			std::vector<glm::vec3> surfacePoints,
			std::vector<glm::vec3> surfaceNormals,
			float curtainHeight
		);

		struct RenderOptions
		{
			bool useWireframe = true;
			glm::vec4 fillColor{};
			glm::vec4 wireframeColor{};
			glm::vec4 lightPosition{};
			glm::vec4 lightColor{};
		};

		void Render(
			RecordState& recordState,
			RenderOptions const& options
		);

		void UpdateGeometry(
			float curtainHeight
		);

		void UpdateGeometry(
			std::vector<glm::vec3> surfacePoints,
			std::vector<glm::vec3> surfaceNormals
		);

		void UpdateGeometry(
			std::vector<glm::vec3> surfacePoints,
			std::vector<glm::vec3> surfaceNormals,
			float curtainHeight
		);

		[[nodiscard]]
		std::vector<CollisionTriangle> const & GetCollisionTriangles() const;

		// Index is the same with the collision triangle
		[[nodiscard]]
		glm::vec3 const& GetTriangleProjectionDirection(int triangleIdx);

	private:

		void UpdateGeometry();

		void UpdateCpuVertices();

		void UpdateCpuIndices();

		void UpdateCollisionTriangles();

		void UpdateVertexBuffer(RecordState const& recordState);

		void UpdateIndexBuffer(RecordState const& recordState);

	private:

		std::vector<glm::vec3> _surfacePoints{};
		std::vector<glm::vec3> _surfaceNormals{};
		float _curtainHeight{};

		std::shared_ptr<Pipeline> _colorPipeline{};
		std::shared_ptr<Pipeline> _wireFramePipeline{};

		std::vector<Pipeline::Vertex> _vertices{};
		std::vector<Index> _indices{};
		int _bufferDirtyCounter = 0;

		std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _vertexBuffers{};
		std::vector<size_t> _vertexBufferSizes{};

		std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _indexBuffers{};
		std::vector<size_t> _indexBufferSizes{};

		std::vector<std::tuple<int, int, int>> _triangles{};
		std::unordered_map<int, std::vector<int>> _vertexNeighbourTriangles{}; 
		std::vector<glm::vec3> _triangleNormals{};

		std::vector<glm::vec3> _triProjDir{};									// Triangles projection direction

		std::vector<CollisionTriangle> _collisionTriangles{};
	};
}
