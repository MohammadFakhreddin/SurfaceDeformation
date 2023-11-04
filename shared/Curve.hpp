#pragma once

#include <vec3.hpp>
#include <vector>

namespace shared::Curve
{

	class LinearCurve
	{
	public:

		explicit LinearCurve(
			std::vector<glm::vec3> points,
			std::vector<glm::vec3> normals
		);

		void Sample(
			float distance, 
			glm::vec3 & outPoint, 
			glm::vec3 & outNormal
		) const;

		[[nodiscard]]
		float GetTotalDistance() const;

	private:

		std::vector<glm::vec3> _points{};
		std::vector<glm::vec3> _normals{};
		std::vector<float> _distances{};

		float _totalDistance{};

	};

	void UniformSample(
		std::vector<glm::vec3> const & inputPoints,
		std::vector<glm::vec3> const & inputNormals,

		std::vector<glm::vec3> & outputPoints,
		std::vector<glm::vec3> & outputNormals,

		float deltaS
	);

};