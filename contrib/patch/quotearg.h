/* quote.h -- declarations for quoting system arguments */

#if defined __STDC__ || __GNUC__
# define __QUOTEARG_P(args) args
#else
# define __QUOTEARG_P(args) ()
#endif

size_t quote_system_arg __QUOTEARG_P ((char *, char const *));
