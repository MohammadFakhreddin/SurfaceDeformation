#include "BedrockLog.hpp"

#include "BedrockMath.hpp"
#include "BufferTracker.hpp"
#include "BSplineApp.hpp"

int main()
{
    {
        BSplineApp app{};
        app.Run();
    }
    
    return 0;
}
