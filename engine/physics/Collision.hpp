#pragma once

#include <vec3.hpp>
#include <vector>
#include <set>

namespace MFA::Collision
{
    class StaticTriangleGrid;

    struct Triangle
    {
        inline static int nextId = 0;
        explicit Triangle()
        {
            id = ++nextId;
        }

        int id = 0;
        glm::dvec3 center{};
        glm::dvec3 normal{};
        glm::dvec3 edgeNormals[3]{};
        glm::dvec3 edgeVertices[3]{};
        bool selected = false;
    };

    // This function can only handle external collision
    [[nodiscard]]
    bool HasIntersection(
        Triangle const& triangle,
        glm::dvec3 const& currentPos,
        glm::dvec3 const& previousPos,
        glm::dvec3& outCollisionPos,
        double& outTime,
        double epsilon,
        bool checkForBackCollision
    );

    [[nodiscard]]
    bool HasIntersection(
        Triangle const& triangle,
        glm::dvec3 const& currentPos,
        glm::dvec3 const& previousPos,
        glm::dvec3& outCollisionPos,
        double epsilon,
        bool checkForBackCollision
    );

    bool HasDynamicIntersection(
        glm::dvec3 const& pointPrevPos,
        glm::dvec3 const& pointCurrPos,

        glm::dvec3 const& triPrevPos0,
        glm::dvec3 const& triPrevPos1,
        glm::dvec3 const& triPrevPos2,

        glm::dvec3 const& triCurrPos0,
        glm::dvec3 const& triCurrPos1,
        glm::dvec3 const& triCurrPos2,

        double epsilon,
        bool checkForBackCollision
    );

    bool HasDynamicEdgeIntersection(
        glm::dvec3 const& p0Prev,
        glm::dvec3 const& p1Prev,
        glm::dvec3 const& p0Curr,
        glm::dvec3 const& p1Curr,
        glm::dvec3 const& q0Prev,
        glm::dvec3 const& q1Prev,
        glm::dvec3 const& q0Curr,
        glm::dvec3 const& q1Curr,

        double epsilon,
        double& collisionTime,
        bool checkForSegmentIntersection = true
    );
    
    // Continuous collision

    [[nodiscard]]
    bool HasContiniousCollision(
        std::vector<Triangle>& triangles,
        glm::dvec3 const& prevPos,
        glm::dvec3 const& nextPos,
        int& outTriangleIdx,
        glm::dvec3& outTrianglePosition,
        glm::dvec3& outTriangleNormal
    );

    [[nodiscard]]
    Triangle GenerateCollisionTriangle(
        glm::dvec3 const& p0,
        glm::dvec3 const& p1,
        glm::dvec3 const& p2
    );

    void UpdateCollisionTriangle(
        glm::dvec3 const& p0,
        glm::dvec3 const& p1,
        glm::dvec3 const& p2,
        Triangle& outTriangle
    );

    // Triangle and point should be on the same plane
    [[nodiscard]]
    bool IsInsideTriangle(Triangle const& triangle, glm::dvec3 const& point);

}

namespace MFA
{
    using CollisionTriangle = Collision::Triangle;
    using StaticCollisionGrid = Collision::StaticTriangleGrid;
}
