#pragma once

#include "pipeline/ColorPipeline.hpp"
#include "geometrycentral/surface/meshio.h"
#include "RenderTypes.hpp"
#include "Collision.hpp"

namespace shared
{
    class SurfaceMeshRenderer
    {
    public:

        using Pipeline = MFA::ColorPipeline;
        using RecordState = MFA::RT::CommandRecordState;
    	using Mesh = geometrycentral::surface::ManifoldSurfaceMesh;
        using Geometry = geometrycentral::surface::VertexPositionGeometry;
        using CollisionTriangle = MFA::CollisionTriangle;

    	explicit SurfaceMeshRenderer(
            std::shared_ptr<Pipeline> colorPipeline,
            std::shared_ptr<Pipeline> wireFramePipeline,
			std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Geometry> geometry
        );

        struct RenderOptions
        {
            bool useWireframe = false;
        };

        struct InstanceOptions
        {
            glm::vec4 fillColor{};
            glm::vec4 wireFrameColor{};
            glm::mat4 fillModel{};
            glm::mat4 wireFrameModel{};
        };

        void Render(
            RecordState& recordState,
            RenderOptions const& options,
            std::vector<InstanceOptions> const& instances
        );

        void UpdateGeometry(
            std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Geometry> geometry
        );

        [[nodiscard]]
        std::vector<CollisionTriangle> GetCollisionTriangles(glm::mat4 const & model) const;

    private:

        void UpdateGeometry();

        void UpdateCpuVertices();

        void UpdateCpuIndices();

        void UpdateCollisionTriangles();

        void UpdateVertexBuffer(RecordState const& recordState);

        void UpdateIndexBuffer(RecordState const& recordState);

    private:

        using Index = uint32_t;

        std::shared_ptr<Pipeline> _colorPipeline{};
        std::shared_ptr<Pipeline> _wireFramePipeline{};

        std::shared_ptr<geometrycentral::surface::ManifoldSurfaceMesh> _mesh {};
        std::shared_ptr<geometrycentral::surface::VertexPositionGeometry> _geometry {};
        
        std::vector<Pipeline::Vertex> _vertices{};
        std::vector<Index> _indices{};
        int _bufferDirtyCounter = 0;

        std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _vertexBuffers{};
        std::vector<size_t> _vertexBufferSizes {};

        std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _indexBuffers{};
        std::vector<size_t> _indexBufferSizes{};

        std::vector<std::tuple<int, int, int>> _triangles{};
        std::unordered_map<int, std::vector<int>> _vertexNeighbourTriangles{}; // Not used

        std::vector<CollisionTriangle> _collisionTriangles{};

    };
};