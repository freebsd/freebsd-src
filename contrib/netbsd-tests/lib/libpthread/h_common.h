#ifndef H_COMMON_H
#define H_COMMON_H

#include <string.h>

#define PTHREAD_REQUIRE(x) \
    do { \
        int ret = (x); \
        ATF_REQUIRE_MSG(ret == 0, "%s: %s", #x, strerror(ret)); \
    } while (0)

#define PTHREAD_REQUIRE_STATUS(x, v) \
    do { \
        int ret = (x); \
        ATF_REQUIRE_MSG(ret == (v), "%s: %s", #x, strerror(ret)); \
    } while (0)

#endif // H_COMMON_H
