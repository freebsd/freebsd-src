/* argmatch.h -- declarations for matching arguments against option lists */

#if defined __STDC__ || __GNUC__
# define __ARGMATCH_P(args) args
#else
# define __ARGMATCH_P(args) ()
#endif

int argmatch __ARGMATCH_P ((const char *, const char * const *));
void invalid_arg __ARGMATCH_P ((const char *, const char *, int));

extern char const program_name[];
