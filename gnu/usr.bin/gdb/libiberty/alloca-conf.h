/* "Normal" configuration for alloca.  */

#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not __GNUC__ */
#ifdef sparc
#include <alloca.h>
extern char *__builtin_alloca();  /* Stupid include file doesn't declare it */
#else
#ifdef __STDC__
PTR alloca (size_t);
#else
PTR alloca ();			/* must agree with functions.def */
#endif
#endif /* sparc */
#endif /* not __GNUC__ */
