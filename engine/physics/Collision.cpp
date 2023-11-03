#include "Collision.hpp"

#include "BedrockAssert.hpp"
#include "BedrockMath.hpp"

namespace MFA::Collision
{

	//-------------------------------------------------------------------------------------------------

	bool HasIntersection(
		Triangle const& triangle,
		glm::dvec3 const& currentPos_,
		glm::dvec3 const& previousPos_,
		glm::dvec3& outCollisionPos,
		double& outTime,
		double const epsilon,
		bool const checkForBackCollision
	)
	{
		auto currentPos = currentPos_;
		auto previousPos = previousPos_;

		auto const movementVector = currentPos - previousPos;

		auto movementMagnitude = glm::length(movementVector);

		if (movementMagnitude == 0.0)
		{
			return false;
		}

		auto const movementDirection = movementVector / movementMagnitude;

		previousPos -= movementDirection * epsilon;
		currentPos += movementDirection * epsilon;
		movementMagnitude += 2 * epsilon;

		auto const surfaceDot2 = glm::dot(previousPos - triangle.center, triangle.normal);
		auto const surfaceDir2 = surfaceDot2 >= 0.0 ? 1.0f : -1.0f;

		if (checkForBackCollision == false && surfaceDir2 < 0)
		{
			return false;
		}

		auto const surfaceDot1 = glm::dot(currentPos - triangle.center, triangle.normal);
		auto const surfaceDir1 = surfaceDot1 >= 0.0 ? 1.0f : -1.0f;

		if (surfaceDir1 == surfaceDir2)
		{
			return false;
		}

		auto const bottom = glm::dot(movementDirection, triangle.normal);

		if (std::abs(bottom) == 0.0)
		{
			return false;
		}

		auto const top = glm::dot(-previousPos + triangle.center, triangle.normal);

		auto const time = top / bottom;

		if (time < 0.0 || time > movementMagnitude)
		{
			return false;
		}

		outCollisionPos = previousPos + time * movementDirection;

		if (IsInsideTriangle(triangle, outCollisionPos) == false)
		{
			return false;
		}

		outTime = time;

		return true;
	}

	//-------------------------------------------------------------------------------------------------

	bool HasIntersection(
		Triangle const& triangle,
		glm::dvec3 const& currentPos,
		glm::dvec3 const& previousPos,
		glm::dvec3& outCollisionPos,
		double const epsilon,
		bool const checkForBackCollision
	)
	{
		double outTime = 0.0;
		return HasIntersection(
			triangle,
			currentPos,
			previousPos,
			outCollisionPos,
			outTime,
			epsilon,
			checkForBackCollision
		);
	}

	//-------------------------------------------------------------------------------------------------

	bool HasContiniousCollision(
		std::vector<Triangle>& triangles,
		glm::dvec3 const& prevPos,
		glm::dvec3 const& nextPos,
		int& outTriangleIdx,
		glm::dvec3& outTrianglePosition,
		glm::dvec3& outTriangleNormal,
		bool checkForBackCollision
	)
	{
		int collisionCount = 0;
		double leastTime = -1.0;
		// We need to choose closest triangle
		for (int i = 0; i < static_cast<int>(triangles.size()); ++i)
		{
			auto const& triangle = triangles[i];

			glm::dvec3 collisionPos{};
			double time = 0.0;

			if (HasIntersection(
				triangle,
				nextPos,
				prevPos,
				collisionPos,
				time,
				0.0,
				checkForBackCollision
			))
			{
				++collisionCount;
				if (leastTime == -1.0 || time < leastTime)
				{
					leastTime = time;
					outTriangleIdx = i;
					outTriangleNormal = triangle.normal;
					outTrianglePosition = collisionPos;
				}
			}
		}

		return leastTime != -1.0;
	}

	//-------------------------------------------------------------------------------------------------

	Triangle GenerateCollisionTriangle(glm::dvec3 const& p0, glm::dvec3 const& p1, glm::dvec3 const& p2)
	{
		Triangle triangle{};
		UpdateCollisionTriangle(p0, p1, p2, triangle);
		return triangle;
	}

	//-------------------------------------------------------------------------------------------------

	void UpdateCollisionTriangle(
		glm::dvec3 const& p0, 
		glm::dvec3 const& p1, 
		glm::dvec3 const& p2,
		Triangle& outTriangle
	)
	{
		auto const v10 = glm::normalize(p1 - p0);
		auto const v21 = glm::normalize(p2 - p1);
		auto const v02 = glm::normalize(p0 - p2);

		outTriangle.center = (p0 + p1 + p2) / 3.0;
		outTriangle.normal = glm::normalize(glm::cross(v10, v21));

		glm::dvec3 normal1 = glm::normalize(glm::cross(v10, outTriangle.normal));
		glm::dvec3 normal2 = glm::normalize(glm::cross(v21, outTriangle.normal));
		glm::dvec3 normal3 = glm::normalize(glm::cross(v02, outTriangle.normal));
		
		outTriangle.edgeVertices[0] = p0 + normal1 * 1e-5;
		outTriangle.edgeVertices[1] = p1 + normal2 * 1e-5;
		outTriangle.edgeVertices[2] = p2 + normal3 * 1e-5;

		//glm::dvec3 normal1 = (glm::cross(v10, outTriangle.normal));
		//glm::dvec3 normal2 = (glm::cross(v21, outTriangle.normal));
		//glm::dvec3 normal3 = (glm::cross(v02, outTriangle.normal));

		outTriangle.edgeVertices[0] = p0;
		outTriangle.edgeVertices[1] = p1;
		outTriangle.edgeVertices[2] = p2;

		outTriangle.edgeNormals[0] = normal1;
		outTriangle.edgeNormals[1] = normal2;
		outTriangle.edgeNormals[2] = normal3;
	}

	//-------------------------------------------------------------------------------------------------

	bool IsInsideTriangle(Triangle const& triangle, glm::dvec3 const& point)
	{
		for (int i = 0; i < 3; ++i)
		{
			auto const dot = glm::dot(triangle.edgeNormals[i], point - triangle.edgeVertices[i]);
			if (dot > 0)
			{
				return false;
			}
		}
		return true;
	}

	//-------------------------------------------------------------------------------------------------

}
