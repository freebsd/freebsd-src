#ifndef __libgxx_assert_h

extern "C" {
#ifdef __assert_h_recursive
#include_next <assert.h>
#else
/* assert.h on some systems needs stdio.h, in violation of ANSI. */
#include <stdio.h>
#include_next <assert.h>

#define __libgxx_assert_h 1
#endif
}
#endif
