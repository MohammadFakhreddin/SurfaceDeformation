#include "Curve.hpp"

#include <glm/glm.hpp>

namespace shared::Curve
{

	//-------------------------------------------------------------------------------------------------

	LinearCurve::LinearCurve(
		std::vector<glm::vec3> points,
		std::vector<glm::vec3> normals
	)
		: _points(std::move(points))
		, _normals(std::move(normals))
	{
		_totalDistance = 0.0f;
		for (int i = 0; i < static_cast<int>(_points.size()) - 1; ++i)
		{
			_distances.emplace_back(_totalDistance);
			_totalDistance += glm::length(_points[i] - _points[i + 1]);
		}
		_distances.emplace_back(_totalDistance);
	}

	//-------------------------------------------------------------------------------------------------

	void LinearCurve::Sample(
		float const distance,
		glm::vec3& outPoint,
		glm::vec3& outNormal
	) const
	{
		bool foundSegment = false;

		int segmentIdx = 0;
		for (segmentIdx = 0; segmentIdx < static_cast<int>(_distances.size()); ++segmentIdx)
		{
			if (_distances[segmentIdx] > distance)
			{
				foundSegment = true;
				break;
			}
		}
		segmentIdx = segmentIdx - 1;

		if (foundSegment == false)
		{
			outPoint = _points[segmentIdx];
			outNormal = _normals[segmentIdx];
			return;
		}

		auto const prevSegment = segmentIdx;
		auto const prevDistance = _distances[prevSegment];
		auto const& prevPosition = _points[prevSegment];
		auto const& prevNormal = _normals[prevSegment];

		auto const nextSegment = segmentIdx + 1;
		auto const nextDistance = _distances[nextSegment];
		auto const& nextPosition = _points[nextSegment];
		auto const& nextNormal = _normals[nextSegment];

		float const t = (distance - prevDistance) / (nextDistance - prevDistance);

		outPoint = glm::mix(prevPosition, nextPosition, t);
		outNormal = glm::mix(prevNormal, nextNormal, t);
	}

	//-------------------------------------------------------------------------------------------------

	float LinearCurve::GetTotalDistance()
	{
		return _totalDistance;
	}

	//-------------------------------------------------------------------------------------------------

	void UniformSample(
		std::vector<glm::vec3> const& inputPoints,
		std::vector<glm::vec3> const& inputNormals,
		std::vector<glm::vec3>& outputPoints,
		std::vector<glm::vec3>& outputNormals,
		float const deltaS
	)
	{
		outputPoints.clear();
		outputNormals.clear();

		LinearCurve curve{ inputPoints, inputNormals };

		float const totalS = curve.GetTotalDistance();

		float currentS = 0.0f;
		while (currentS <= totalS)
		{
			outputPoints.emplace_back();
			outputNormals.emplace_back();
			curve.Sample(currentS, outputPoints.back(), outputNormals.back());
			currentS += deltaS;
		}
	}

	//-------------------------------------------------------------------------------------------------

}
