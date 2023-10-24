#include "platform/platform_debug.h"
#include "util/debug.h"
#include <features.h>

#undef assert

#ifdef NDEBUG
    #define assert(x) (void)0
#else
    #define assert(x) OC_ASSERT(x)
#endif

#if __STDC_VERSION__ >= 201112L && !defined(__cplusplus)
    #define static_assert _Static_assert
#endif
