#pragma once

#include "Contribution.hpp"
#include "geometrycentral/surface/vertex_position_geometry.h"

namespace shared
{

    std::unique_ptr<ContributionMap> CatmullClarkSubdivide(
        geometrycentral::surface::ManifoldSurfaceMesh& mesh, 
        geometrycentral::surface::VertexPositionGeometry& geo
    );

}
