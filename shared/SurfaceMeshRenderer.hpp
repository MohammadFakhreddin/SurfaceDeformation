#pragma once

#include "pipeline/ColorPipeline.hpp"
#include "geometrycentral/surface/meshio.h"
#include "RenderTypes.hpp"
#include "Collision.hpp"
#include "SurfaceMesh.hpp"

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
        using Index = uint32_t;

    	explicit SurfaceMeshRenderer(
            std::shared_ptr<Pipeline> colorPipeline,
            std::shared_ptr<Pipeline> wireFramePipeline,
            std::shared_ptr<SurfaceMesh> surfaceMesh
        );

        struct RenderOptions
        {
            bool useWireframe = true;
        };

        struct InstanceOptions
        {
            glm::vec4 fillColor{};
            glm::vec4 wireFrameColor{};
            glm::mat4 fillModel{};
            glm::mat4 wireFrameModel{};
            glm::vec4 lightColor{};
            glm::vec4 lightPosition{};
        };

        void Render(
            RecordState& recordState,
            RenderOptions const& options,
            std::vector<InstanceOptions> const& instances
        );

        void UpdateGeometry(std::shared_ptr<SurfaceMesh> surfaceMesh);

        [[nodiscard]]
        std::vector<CollisionTriangle> GetCollisionTriangles(glm::mat4 const & model) const;

        bool GetVertexIndices(int triangleIdx, std::tuple<int, int, int> & outVIds) const;

        bool GetVertexNeighbors(int vertexIdx, std::set<int> & outVIds) const;

        bool GetVertexPosition(int vertexIdx, glm::vec3 & outPosition) const;

        int GetVertexIdx(glm::vec3 const & position) const;

    private:

        void UpdateGeometry();

        void UpdateVertexBuffer(RecordState const& recordState);

        void UpdateIndexBuffer(RecordState const& recordState);

    private:

        std::shared_ptr<Pipeline> _colorPipeline{};
        std::shared_ptr<Pipeline> _wireFramePipeline{};

        std::shared_ptr<SurfaceMesh> _surfaceMesh{};

        //std::shared_ptr<geometrycentral::surface::ManifoldSurfaceMesh> _mesh {};
        //std::shared_ptr<geometrycentral::surface::VertexPositionGeometry> _geometry {};
        
        //std::vector<Pipeline::Vertex> _vertices{};
        //std::vector<Index> _indices{};
        int _bufferDirtyCounter = 0;

        std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _vertexBuffers{};
        std::vector<size_t> _vertexBufferSizes {};

        std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _indexBuffers{};
        std::vector<size_t> _indexBufferSizes{};

        //std::vector<std::tuple<int, int, int>> _triangles{};
        //std::unordered_map<int, std::vector<int>> _vertexNeighbourTriangles{};
        //std::unordered_map<int, std::set<int>> _vertexNeighbourVertices{};
        //std::vector<glm::vec3> _triangleNormals{};

        //std::vector<CollisionTriangle> _collisionTriangles{};

    };
};