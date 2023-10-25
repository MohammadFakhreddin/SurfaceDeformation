#pragma once

#include <glm/glm.hpp>
#include <vector>
// TODO: Implementing a BSpline curve is a different assignment
namespace Shared::BSplineCurve
{

    using ControlPoints = std::vector<glm::vec3>;
    using KnotSequence = std::vector<float>;
    using Curve = std::vector<glm::vec3>;

    void GenerateCurve(
        ControlPoints const & cps, 
        KnotSequence const & knotSeq,
        float const uStep, 
        int const order,
        Curve & outCurve
    )
    {

    }

    void GenerateUniformKnotSequence()
    {
         

    }

}