#if !defined SAVEDIR_H_
# define SAVEDIR_H_

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

char *
savedir PARAMS ((const char *dir, off_t name_size));

#endif
