#include "SurfaceMesh.hpp"

#include "BedrockAssert.hpp"

#include <ext/scalar_constants.hpp>

using namespace MFA;

namespace shared
{
    
    //------------------------------------------------------------

    SurfaceMesh::SurfaceMesh(
        std::shared_ptr<Mesh> mesh,
        std::shared_ptr<Geometry> geometry
    )
        : _mesh(std::move(mesh))
	    , _geometry(std::move(geometry))
    {
        UpdateGeometry();
    }

    //------------------------------------------------------------

    void SurfaceMesh::UpdateGeometry(
        std::shared_ptr<Mesh> mesh,
        std::shared_ptr<Geometry> geometry
    )
    {
        _mesh = std::move(mesh);
	    _geometry = std::move(geometry);
	    UpdateGeometry();
    }

    //------------------------------------------------------------

    std::vector<MFA::CollisionTriangle> SurfaceMesh::GetCollisionTriangles(glm::mat4 const & model) const
    {
        std::vector<CollisionTriangle> result = _collisionTriangles;

        #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(result.size()); ++i)
        {
            auto& triangle = result[i];

            triangle.normal = glm::normalize(model * glm::vec4{ triangle.normal, 0.0f });
            triangle.center = model * glm::vec4{ triangle.center, 1.0f };

            for (auto& edgeNormal : triangle.edgeNormals)
            {
                edgeNormal = glm::normalize(model * glm::vec4{ edgeNormal, 0.0f });
            }
            for (auto& edgeVertex : triangle.edgeVertices)
            {
                edgeVertex = model * glm::vec4{ edgeVertex, 1.0f };
            }
        }

        return result;
    }

    //------------------------------------------------------------

    bool SurfaceMesh::GetVertexIndices(int triangleIdx, std::tuple<int, int, int> & outVIds) const
    {
        if (triangleIdx < 0 || triangleIdx >= _triangles.size())
        {
            return false;
        }
        outVIds = _triangles[triangleIdx];
        return true;
    }
    
    //------------------------------------------------------------

    bool SurfaceMesh::GetVertexNeighbors(int vertexIdx, std::set<int> & outVIds) const
    {
        auto const findResult = _vertexNeighbourVertices.find(vertexIdx);
        if (findResult != _vertexNeighbourVertices.end())
        {
            outVIds = findResult->second;
            return true;
        }
        return false;
    }

    //------------------------------------------------------------

    bool SurfaceMesh::GetVertexPosition(int vertexIdx, glm::vec3 & outPosition) const
    {
        if (vertexIdx < 0 || vertexIdx >= static_cast<int>(_vertices.size()))
        {
            return false;
        }
        outPosition = _vertices[vertexIdx].position;
        return true;
    }

    //------------------------------------------------------------

    int SurfaceMesh::GetVertexIdx(glm::vec3 const & position) const
    {
        for (int i = 0; i < static_cast<int>(_vertices.size()); ++i)
        {
            auto const& vPos = _vertices[i];
            auto const distance2 =
                std::pow(vPos.position.x - position.x, 2) +
                std::pow(vPos.position.y - position.y, 2) +
                std::pow(vPos.position.z - position.z, 2);

            if (distance2 < glm::epsilon<float>() * glm::epsilon<float>())
            {
                return i;
            }
        }
        return -1;
    }
    
    //------------------------------------------------------------

    void SurfaceMesh::UpdateGeometry()
    {
        UpdateCpuIndices();
        UpdateCpuVertices();
        UpdateCollisionTriangles();
    }

    //------------------------------------------------------------

    void SurfaceMesh::UpdateCpuVertices()
    {
        _vertices.clear();
        _triangleNormals.clear();

        auto const positions = _geometry->vertexPositions.toVector();

        for (int i = 0; i < static_cast<int>(_triangles.size()); ++i)
        {
            auto const& [idx0, idx1, idx2] = _triangles[i];

            glm::vec3 const v0 = glm::vec3{ positions[idx0].x, positions[idx0].y, positions[idx0].z };
            glm::vec3 const v1 = glm::vec3{ positions[idx1].x, positions[idx1].y, positions[idx1].z };
            glm::vec3 const v2 = glm::vec3{ positions[idx2].x, positions[idx2].y, positions[idx2].z };

            auto const cross = glm::normalize(glm::cross(v1 - v0, v2 - v1));

            _triangleNormals.emplace_back(cross);
        }

        for (int vIdx = 0; vIdx < positions.size(); ++vIdx)
        {
            glm::vec3 normal{};
            for (auto const& triIdx : _vertexNeighbourTriangles[vIdx])
            {
                normal += _triangleNormals[triIdx];
            }
            normal = glm::normalize(normal);
            
            _vertices.emplace_back(Pipeline::Vertex{
                .position = glm::vec3 {positions[vIdx].x, positions[vIdx].y, positions[vIdx].z},
                .normal = normal
            });
        }
    }

    //------------------------------------------------------------

    void SurfaceMesh::UpdateCpuIndices()
    {
        _indices.clear();
        _vertexNeighbourTriangles.clear();
        _vertexNeighbourVertices.clear();
        _triangles.clear();
        
        auto const faceVertexList = _mesh->getFaceVertexList();
        for (auto const& faceVertices : faceVertexList)
        {
            if (faceVertices.size() == 3)
            {
                int idx0 = faceVertices[0];
                int idx1 = faceVertices[1];
                int idx2 = faceVertices[2];

                _indices.emplace_back(idx0);
                _indices.emplace_back(idx1);
                _indices.emplace_back(idx2);

                _triangles.emplace_back(std::tuple{ idx0, idx1, idx2 });

                _vertexNeighbourTriangles[idx0].emplace_back(_triangles.size() - 1);
                _vertexNeighbourTriangles[idx1].emplace_back(_triangles.size() - 1);
                _vertexNeighbourTriangles[idx2].emplace_back(_triangles.size() - 1);

                _vertexNeighbourVertices[idx0].emplace(idx1);
                _vertexNeighbourVertices[idx0].emplace(idx2);
                _vertexNeighbourVertices[idx1].emplace(idx2);
                _vertexNeighbourVertices[idx1].emplace(idx0);
                _vertexNeighbourVertices[idx2].emplace(idx0);
                _vertexNeighbourVertices[idx2].emplace(idx1);
            }
            else if (faceVertices.size() == 4)
            {
                {// Triangle0
                    int idx0 = faceVertices[0];
                    int idx1 = faceVertices[1];
                    int idx2 = faceVertices[2];

                    _indices.emplace_back(idx0);
                    _indices.emplace_back(idx1);
                    _indices.emplace_back(idx2);
                    
                    _triangles.emplace_back(std::tuple{ idx0, idx1, idx2 });

                    _vertexNeighbourTriangles[idx0].emplace_back(_triangles.size() - 1);
                    _vertexNeighbourTriangles[idx1].emplace_back(_triangles.size() - 1);
                    _vertexNeighbourTriangles[idx2].emplace_back(_triangles.size() - 1);

                    _vertexNeighbourVertices[idx0].emplace(idx1);
                    _vertexNeighbourVertices[idx0].emplace(idx2);
                    _vertexNeighbourVertices[idx1].emplace(idx2);
                    _vertexNeighbourVertices[idx1].emplace(idx0);
                    _vertexNeighbourVertices[idx2].emplace(idx0);
                    _vertexNeighbourVertices[idx2].emplace(idx1);
                }
                {// Triangle1
                    int idx0 = faceVertices[2];
                    int idx1 = faceVertices[3];
                    int idx2 = faceVertices[0];

                    _indices.emplace_back(idx0);
                    _indices.emplace_back(idx1);
                    _indices.emplace_back(idx2);

                    _triangles.emplace_back(std::tuple{ idx0, idx1, idx2 });

                    _vertexNeighbourTriangles[idx0].emplace_back(_triangles.size() - 1);
                    _vertexNeighbourTriangles[idx1].emplace_back(_triangles.size() - 1);
                    _vertexNeighbourTriangles[idx2].emplace_back(_triangles.size() - 1);

                    _vertexNeighbourVertices[idx0].emplace(idx1);
                    _vertexNeighbourVertices[idx0].emplace(idx2);
                    _vertexNeighbourVertices[idx1].emplace(idx2);
                    _vertexNeighbourVertices[idx1].emplace(idx0);
                    _vertexNeighbourVertices[idx2].emplace(idx0);
                    _vertexNeighbourVertices[idx2].emplace(idx1);
                }
            }
            else
            {
                MFA_ASSERT(false);
            }
        }
    }

    //------------------------------------------------------------

    void SurfaceMesh::UpdateCollisionTriangles()
    {
        _collisionTriangles.resize(_triangles.size());
        #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(_triangles.size()); ++i)
        {
            auto [idx0, idx1, idx2] = _triangles[i];

            auto const& v0 = _vertices[idx0].position;
            auto const& v1 = _vertices[idx1].position;
            auto const& v2 = _vertices[idx2].position;

            Collision::UpdateCollisionTriangle(
                v0,
                v1,
                v2,
                _collisionTriangles[i]
            );
        }
    }

    //------------------------------------------------------------

    std::shared_ptr<SurfaceMesh::Mesh> const& SurfaceMesh::GetMesh()
    {
        return _mesh;
    }

    //------------------------------------------------------------

    std::shared_ptr<SurfaceMesh::Geometry> const& SurfaceMesh::GetGeometry()
    {
        return _geometry;
    }

    //------------------------------------------------------------

    std::vector<ColorPipeline::Vertex> & SurfaceMesh::GetVertices()
    {
        return _vertices;
    }

    //------------------------------------------------------------

    std::vector<SurfaceMesh::Index> & SurfaceMesh::GetIndices()
    {
        return _indices;
    }

    //------------------------------------------------------------

}
