#pragma once

#include <vector>
#include <tuple>
#include <unordered_map>
#include <Eigen/Eigen>

#include <geometrycentral/utilities/vector3.h>

#include "geometrycentral/surface/halfedge_element_types.h"

namespace shared
{
    using Vector3 = geometrycentral::Vector3;
    using Vertex = geometrycentral::surface::Vertex;
    // TODO: Rewrite this in a more meaningful well
    using ContribTuple = std::tuple<Vertex, Vertex, float>;   //Old v to new V

    struct Contribution
    {
    public:
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
        std::vector<Contribution *> const& GetPrevLvlContibs(int prevGIdx);

        [[nodiscard]]
        std::vector<Contribution *> const& GetNextLvlContribs(int nextGIdx);

    private:

        [[nodiscard]]
        static int GetVertexIdx(
            Vertices const& vertices,
            Vector3 const& position
        );

        std::vector<Contribution> _contribs{};
        
        std::unordered_map<int, std::vector<Contribution *>> _prevLvlContribIdx{};
        std::unordered_map<int, std::vector<Contribution *>> _nextLvlContribIdx{};

    };

}
