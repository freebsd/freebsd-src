#ifndef XSTRTOD_H
#define XSTRTOD_H 1

#ifndef __P
# if defined (__GNUC__) || (defined (__STDC__) && __STDC__)
#  define __P(args) args
# else
#  define __P(args) ()
# endif  /* GCC.  */
#endif  /* Not __P.  */

int
  xstrtod __P ((const char *str, const char **ptr, double *result));

#endif /* XSTRTOD_H */
