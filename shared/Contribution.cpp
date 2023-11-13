#include "Contribution.hpp"

#include <ext/scalar_constants.hpp>

#include <omp.h>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/util/Constants.h>

#include "BedrockAssert.hpp"
#include "geometrycentral/utilities/vector3.h"

namespace shared
{

	//-----------------------------------------------------------------------------------------

	ContributionMap::ContributionMap(
		Vertices const& prevLvlVs,
		Vertices const& nextLvlVs,
		std::vector<ContribTuple> const& prevToNextContrib
	)
	{
		_contribs.resize(prevToNextContrib.size());
		#pragma omp parallel for
		for (int i = 0; i < static_cast<int>(prevToNextContrib.size()); ++i)
		{
			auto const& [prevLvlPos, nextLvlPos, value] = prevToNextContrib[i];

			auto const prevIdx = GetVertexIdx(prevLvlVs, prevLvlPos);
			auto const nextIdx = GetVertexIdx(nextLvlVs, nextLvlPos);

			auto const contribIdx = i;

			_contribs[contribIdx] = Contribution{
				.nextLvlVIdx = prevIdx,
				.prevLvlVIdx = nextIdx,
				.amount = value
			};
		}

		for (int i = 0; i < static_cast<int>(_contribs.size()); ++i)
		{
			auto const& contrib = _contribs[i];

			auto const prevIdx = contrib.prevLvlVIdx;
			auto const nextIdx = contrib.nextLvlVIdx;
			auto const contribIdx = i;

			auto fPrevRes = _prevLvlContribIdx.find(prevIdx);
			if (fPrevRes != _prevLvlContribIdx.end())
			{
				fPrevRes->second.emplace_back(contribIdx);
			}
			else
			{
				_prevLvlContribIdx[prevIdx] = { contribIdx };
			}

			auto fNextRes = _nextLvlContribIdx.find(nextIdx);
			if (fNextRes != _nextLvlContribIdx.end())
			{
				fNextRes->second.emplace_back(contribIdx);
			}
			else
			{
				_nextLvlContribIdx[nextIdx] = { contribIdx };
			}
		}
	}

	//-----------------------------------------------------------------------------------------

	int ContributionMap::GetVertexIdx(
		Vertices const& vertices, 
		Vector3 const& position
	)
	{
		for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
		{
			auto const distance2 =
				std::pow(vertices[i].x - position.x, 2) +
				std::pow(vertices[i].y - position.y, 2) +
				std::pow(vertices[i].z - position.z, 2);

			if (distance2 < glm::epsilon<float>() * glm::epsilon<float>())
			{
				return i;
			}
		}
		MFA_ASSERT(false);
		return -1;
	}

	//-----------------------------------------------------------------------------------------

}
