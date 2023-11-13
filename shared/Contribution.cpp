#include "Contribution.hpp"

#include <ext/scalar_constants.hpp>

#include <omp.h>

#include "BedrockAssert.hpp"

namespace shared
{

	//-----------------------------------------------------------------------------------------

	ContributionMap::ContributionMap(
		std::vector<glm::vec3> const& prevLvlVs,
		std::vector<glm::vec3> const& nextLvlVs,
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

			auto fPrevRes = _prevLvlContribIdx.find(prevIdx);
			if (fPrevRes != _prevLvlContribIdx.end())
			{
				fPrevRes->second.emplace_back(i);
			}
			else
			{
				_prevLvlContribIdx[prevIdx] = { i };
			}
			
			auto fNextRes = _nextLvlContribIdx.find(nextIdx);
			if (fNextRes != _nextLvlContribIdx.end())
			{
				fNextRes->second.emplace_back(i);
			}
			else
			{
				_nextLvlContribIdx[nextIdx] = { i };
			}
			
			_contribs[i] = Contribution{
				.nextLvlVIdx = prevIdx,
				.prevLvlVIdx = nextIdx,
				.amount = value
			};
		}
	}

	//-----------------------------------------------------------------------------------------

	int ContributionMap::GetVertexIdx(
		std::vector<glm::vec3> const& vertices, 
		glm::vec3 const& position
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
