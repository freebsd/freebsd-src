#pragma once

#include "sys/types.h"

struct group {
  char *gr_name;
  gid_t gr_gid;
  char *gr_passwd;
  char **gr_mem;
};
