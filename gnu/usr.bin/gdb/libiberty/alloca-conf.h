/* "Normal" configuration for alloca.  */

#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not __GNUC__ */
#ifdef sparc
#include <alloca.h>
#else
char *alloca ();
#endif /* sparc */
#endif /* not __GNUC__ */
