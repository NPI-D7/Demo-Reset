#ifndef CORE_PRIVATE_H
#define CORE_PRIVATE_H

#pragma once
#include <3ds.h>
#include <atomic>

namespace m3d {
    namespace priv {
        namespace core {
            extern std::atomic<bool> ledPatternRunning;
            extern Handle ptmsysmHandle;
        } /* core */
    } /* priv */
} /* m3d */


#endif /* end of include guard: CORE_PRIVATE_H */
