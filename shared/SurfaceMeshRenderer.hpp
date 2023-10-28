#pragma once

#include "pipeline/ColorPipeline.hpp"
#include "geometrycentral/surface/meshio.h"
#include "RenderTypes.hpp"

namespace shared
{
    class SurfaceMeshRenderer
    {
    public:

        using Pipeline = MFA::ColorPipeline;
        using RecordState = MFA::RT::CommandRecordState;
    	using Mesh = geometrycentral::surface::ManifoldSurfaceMesh;
        using Geometry = geometrycentral::surface::VertexPositionGeometry;
        
    	explicit SurfaceMeshRenderer(
            std::shared_ptr<Pipeline> colorPipeline,
            std::shared_ptr<Pipeline> wireFramePipeline,
			std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Geometry> geometry,
            glm::vec4 const & fillColor,
            glm::vec4 const & wireFrameColor
        );

        struct RenderOptions
        {
            bool useWireframe = false;
        };

        void Render(RecordState & recordState, RenderOptions const& options);

        void UpdateGeometry(
            std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Geometry> geometry
        );

    private:

        void UpdateGeometry();

        void UpdateCpuVertices();

        void UpdateCpuIndices();

        void UpdateVertexBuffer(RecordState const& recordState);

        void UpdateIndexBuffer(RecordState const& recordState);

    private:

        using Index = uint32_t;

        std::shared_ptr<Pipeline> _colorPipeline{};
        std::shared_ptr<Pipeline> _wireFramePipeline{};

        std::shared_ptr<geometrycentral::surface::ManifoldSurfaceMesh> _mesh {};
        std::shared_ptr<geometrycentral::surface::VertexPositionGeometry> _geometry {};

        glm::vec4 _fillColor{};
        glm::vec4 _wireFrameColor{};

        std::vector<Pipeline::Vertex> _vertices{};
        std::vector<Index> _indices{};
        int _bufferDirtyCounter = 0;

        std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _vertexBuffers{};
        std::vector<size_t> _vertexBufferSizes {};

        std::vector<std::shared_ptr<MFA::RT::BufferAndMemory>> _indexBuffers{};
        std::vector<size_t> _indexBufferSizes{};
        // TODO: These are not used
        std::vector<std::tuple<int, int, int>> _triangles{};
        std::unordered_map<int, std::vector<int>> _vertexNeighbourTriangles{};
    
    };
};