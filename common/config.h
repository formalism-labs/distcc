
#if defined(_WIN32)
#include "config/windows/config.h"

#elif defined(__CYGWIN__)
#include "config/cygwin/config.h"

#elif defined(__linux__)
#include "config/linux/config.h"
#endif
