#pragma once

#include <glm/glm.hpp>

namespace Shared
{

    class ControlPoint
    {
    public:
    
        explicit ControlPoint(glm::vec3 position);

        [[nodiscard]]
        glm::vec3 const & GetPosition();

        void SetPosition(glm::vec3 position);

    private:

        int _idx{};
        glm::vec3 _position{};
        float _weight = 1.0f;
        
    };

};