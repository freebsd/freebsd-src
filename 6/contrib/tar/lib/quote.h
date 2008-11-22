/* prototypes for quote.c */

#ifndef PARAMS
# if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

char const *quote_n PARAMS ((int n, char const *name));
char const *quote PARAMS ((char const *name));
