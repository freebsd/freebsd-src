#ifndef __libgxx_stddef_h

extern "C" {
#ifdef __stddef_h_recursive
#include_next <stddef.h>
#else
#include_next <stddef.h>

#define __libgxx_stddef_h 1
#endif
}
#endif
