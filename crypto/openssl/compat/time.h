/*
 * Public domain
 * sys/time.h compatibility shim
 */

#ifdef _MSC_VER
#include <../include/time.h>
#define gmtime_r(tp, tm) ((gmtime_s((tm), (tp)) == 0) ? (tm) : NULL)
#else
#include_next <time.h>
#endif
