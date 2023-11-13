#pragma once

#include "pipeline/ColorPipeline.hpp"
#include "geometrycentral/surface/meshio.h"
#include "Collision.hpp"

#include <vector>
#include <unordered_map>

namespace shared
{
    class SurfaceMesh
    {
    public:

        using Mesh = geometrycentral::surface::ManifoldSurfaceMesh;
        using Geometry = geometrycentral::surface::VertexPositionGeometry;
        using CollisionTriangle = MFA::CollisionTriangle;
        using Pipeline = MFA::ColorPipeline;
        using Vertex = Pipeline::Vertex;
        using Index = uint32_t;
        
        explicit SurfaceMesh(
            std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Geometry> geometry
        );

        void UpdateGeometry(
            std::shared_ptr<Mesh> mesh,
            std::shared_ptr<Geometry> geometry
        );

        [[nodiscard]]
        std::vector<CollisionTriangle> GetCollisionTriangles(glm::mat4 const & model) const;

        bool GetVertexIndices(int triangleIdx, std::tuple<int, int, int> & outVIds) const;

        bool GetVertexNeighbors(int vertexIdx, std::set<int> & outVIds) const;

        bool GetVertexPosition(int vertexIdx, glm::vec3 & outPosition) const;

        int GetVertexIdx(glm::vec3 const & position) const;

        void UpdateGeometry();

        void UpdateCpuVertices();

        void UpdateCpuIndices();

        void UpdateCollisionTriangles();

        [[nodiscard]]
    	std::shared_ptr<Mesh> const& GetMesh();

        [[nodiscard]]
        std::shared_ptr<Geometry> const& GetGeometry();

        [[nodiscard]]
        std::vector<Pipeline::Vertex> & GetVertices();

        [[nodiscard]]
        std::vector<Index> & GetIndices();

    private:

        std::shared_ptr<Mesh> _mesh {};
        std::shared_ptr<Geometry> _geometry {};
        
        std::vector<Pipeline::Vertex> _vertices{};
        std::vector<Index> _indices{};

        std::vector<std::tuple<int, int, int>> _triangles{};
        std::unordered_map<int, std::vector<int>> _vertexNeighbourTriangles{};
        std::unordered_map<int, std::set<int>> _vertexNeighbourVertices{};
        std::vector<glm::vec3> _triangleNormals{};

        std::vector<CollisionTriangle> _collisionTriangles{};
    };
};