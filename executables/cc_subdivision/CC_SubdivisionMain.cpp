#include "BedrockLog.hpp"

#include "BedrockMath.hpp"
#include "BufferTracker.hpp"
#include "CC_SubdivisionApp.hpp"

int main()
{
    {
        CC_SubdivisionApp app{};
        app.Run();
    }
    
    return 0;
}
