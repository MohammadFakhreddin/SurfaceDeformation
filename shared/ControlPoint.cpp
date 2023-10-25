#include "ControlPoint.hpp"

namespace Shared
{
    
    static inline int nextIdx = 0;    

    //----------------------------------------------------

    ControlPoint::ControlPoint(glm::vec3 position)
        : _position(std::move(position))
        , _idx(nextIdx++)
    {}
    
    //----------------------------------------------------

    glm::vec3 const & ControlPoint::GetPosition()
    {
        return _position;
    }

    //----------------------------------------------------

    void ControlPoint::SetPosition(glm::vec3 position)
    {
        _position = std::move(position);
    }

    //----------------------------------------------------

};