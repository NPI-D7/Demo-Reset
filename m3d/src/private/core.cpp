#include "m3d/private/core.hpp"

namespace m3d {
    namespace priv {
        namespace core {
            std::atomic<bool> ledPatternRunning(false);
            Handle ptmsysmHandle = 0;
        } /* core */
    } /* priv */
} /* m3d */
