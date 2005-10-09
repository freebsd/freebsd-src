#ifndef GETSTR_H_
# define GETSTR_H_ 1

# include <stdio.h>

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

int
getstr PARAMS ((char **lineptr, size_t *n, FILE *stream,
		int delim1, int delim2,
		size_t offset));

#endif
