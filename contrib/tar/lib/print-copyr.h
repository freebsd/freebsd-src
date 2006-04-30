# ifndef PARAMS
#  if PROTOTYPES || (defined (__STDC__) && __STDC__)
#   define PARAMS(args) args
#  else
#   define PARAMS(args) ()
#  endif
# endif

void print_copyright PARAMS((char const *));
