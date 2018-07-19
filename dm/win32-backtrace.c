#include "config.h"

#ifndef LIBIMG

initcall(backtrace_init)
{
    if (!LoadLibraryA("uxen-backtrace.dll"))
        Wwarn("LoadLibraryA(uxen-backtrace.dll) failed");
}

#endif
