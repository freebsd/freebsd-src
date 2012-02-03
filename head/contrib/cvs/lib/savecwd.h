#ifndef SAVE_CWD_H
#define SAVE_CWD_H 1

struct saved_cwd
  {
    int desc;
    char *name;
  };

#if defined (__GNUC__) || (defined (__STDC__) && __STDC__)
#define __PROTO(args) args
#else
#define __PROTO(args) ()
#endif  /* GCC.  */

int save_cwd __PROTO((struct saved_cwd *cwd));
int restore_cwd __PROTO((const struct saved_cwd *cwd, const char *dest));
void free_cwd __PROTO((struct saved_cwd *cwd));

#endif /* SAVE_CWD_H */
