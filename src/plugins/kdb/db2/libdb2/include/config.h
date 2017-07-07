/* This includes autoconf.h and defines u_int32_t.  */
#include "db-config.h"

#ifndef HAVE_U_INT16_T
#define u_int16_t unsigned short
#endif
#ifndef HAVE_INT16_T
#define int16_t short
#endif

#ifndef HAVE_INT8_T
#define int8_t signed char
#endif
#ifndef HAVE_U_INT8_T
#define u_int8_t unsigned char
#endif
#ifndef HAVE_INT32_T
#define int32_t int
#endif

#ifndef HAVE_SSIZE_T
#define ssize_t int
#endif
