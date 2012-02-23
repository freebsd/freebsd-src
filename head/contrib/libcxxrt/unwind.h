#ifndef UNWIND_H_INCLUDED
#define UNWIND_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __arm__
#include "unwind-arm.h"
#else
#include "unwind-itanium.h"
#endif

#ifdef __cplusplus
}
#endif

#endif
