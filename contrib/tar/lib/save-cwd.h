#ifndef SAVE_CWD_H
# define SAVE_CWD_H 1

struct saved_cwd
  {
    int desc;
    char *name;
  };

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

int save_cwd PARAMS ((struct saved_cwd *cwd));
int restore_cwd PARAMS ((const struct saved_cwd *cwd, const char *dest,
			 const char *from));
void free_cwd PARAMS ((struct saved_cwd *cwd));

#endif /* SAVE_CWD_H */
