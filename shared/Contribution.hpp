#pragma once

#include <vector>
#include <tuple>
#include <vec3.hpp>
#include <unordered_map>

namespace shared
{
    // TODO: Rewrite this in a more meaningful well
    using ContribTuple = std::tuple<glm::vec3, glm::vec3, float>;

    struct Contribution
    {
        int nextLvlVIdx = -1;			// Next level vertex idx
        int prevLvlVIdx = -1;			// Prev level vertex idx
        float amount = 0.0f;			// Contribution amount
    };

    class ContributionMap
    {
    public:

        explicit ContributionMap(
            std::vector<glm::vec3> const & prevLvlVs,                                           // Previous lvl vertices
            std::vector<glm::vec3> const & nextLvlVs,                                           // Next lvl vertices
            std::vector<ContribTuple> const & prevToNextContrib      // Prev lvl to next lvl contribution
        );

        [[nodiscard]]
        static int GetVertexIdx(
            std::vector<glm::vec3> const & vertices,
            glm::vec3 const& position
        );

    private:

        std::vector<Contribution> _contribs{};
        
        std::unordered_map<int, std::vector<int>> _prevLvlContribIdx{};
        std::unordered_map<int, std::vector<int>> _nextLvlContribIdx{};

    };

}