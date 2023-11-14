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

			auto const prevIdx = prevLvlPos.getIndex();// GetVertexIdx(prevLvlVs, prevLvlPos);
			auto const nextIdx = nextLvlPos.getIndex();//GetVertexIdx(nextLvlVs, nextLvlPos);

			auto const contribIdx = i;

			_contribs[contribIdx] = Contribution{
				.nextLvlVIdx = static_cast<int>(nextIdx),
				.prevLvlVIdx = static_cast<int>(prevIdx),
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
				fPrevRes->second.emplace_back(&_contribs[contribIdx]);
			}
			else
			{
				_prevLvlContribIdx[prevIdx] = { &_contribs[contribIdx] };
			}

			auto fNextRes = _nextLvlContribIdx.find(nextIdx);
			if (fNextRes != _nextLvlContribIdx.end())
			{
				fNextRes->second.emplace_back(&_contribs[contribIdx]);
			}
			else
			{
				_nextLvlContribIdx[nextIdx] = { &_contribs[contribIdx] };
			}
		}
	}

	//-----------------------------------------------------------------------------------------

	std::vector<Contribution *> const& ContributionMap::GetPrevLvlContibs(int const prevGIdx)
	{
		MFA_ASSERT(_prevLvlContribIdx.contains(prevGIdx));
		return _prevLvlContribIdx[prevGIdx];
	}

	//-----------------------------------------------------------------------------------------

	std::vector<Contribution *> const& ContributionMap::GetNextLvlContribs(int const nextGIdx)
	{
		MFA_ASSERT(_nextLvlContribIdx.contains(nextGIdx));
		return _nextLvlContribIdx[nextGIdx];
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
