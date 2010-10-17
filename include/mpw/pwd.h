#ifndef __pwd_h
#define __pwd_h

#include <sys/types.h>

struct passwd {
  char *pw_name;
  uid_t pw_uid;
  gid_t pw_gid;
  char *pw_dir;
  char *pw_shell;
  char *pw_passwd;
};

#endif /* ! __pwd_h */
