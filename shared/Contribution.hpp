#pragma once

#include <vector>
#include <tuple>
#include <unordered_map>
#include <Eigen/Eigen>

#include <geometrycentral/utilities/vector3.h>

namespace shared
{
    using Vector3 = geometrycentral::Vector3;
    // TODO: Rewrite this in a more meaningful well
    using ContribTuple = std::tuple<Vector3, Vector3, float>;

    struct Contribution
    {
        int nextLvlVIdx = -1;			// Next level vertex idx
        int prevLvlVIdx = -1;			// Prev level vertex idx
        float amount = 0.0f;			// Contribution amount
    };

    class ContributionMap
    {
    public:

        using Vertices = Eigen::Matrix<Vector3, Eigen::Dynamic, 1>;

        explicit ContributionMap(
            Vertices const& prevLvlVs,                                           // Previous lvl vertices
            Vertices const & nextLvlVs,                                           // Next lvl vertices
            std::vector<ContribTuple> const & prevToNextContrib      // Prev lvl to next lvl contribution
        );

        [[nodiscard]]
        static int GetVertexIdx(
            Vertices const & vertices,
            Vector3 const& position
        );

    private:

        std::vector<Contribution> _contribs{};
        
        std::unordered_map<int, std::vector<int>> _prevLvlContribIdx{};
        std::unordered_map<int, std::vector<int>> _nextLvlContribIdx{};

    };

}
