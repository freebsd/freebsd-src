/* basename.c -- return the last element in a path */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <backupfile.h>

#ifndef FILESYSTEM_PREFIX_LEN
#define FILESYSTEM_PREFIX_LEN(f) 0
#endif

#ifndef ISSLASH
#define ISSLASH(c) ((c) == '/')
#endif

/* In general, we can't use the builtin `basename' function if available,
   since it has different meanings in different environments.
   In some environments the builtin `basename' modifies its argument.  */

char *
base_name (name)
     char const *name;
{
  char const *base = name += FILESYSTEM_PREFIX_LEN (name);

  for (; *name; name++)
    if (ISSLASH (*name))
      base = name + 1;

  return (char *) base;
}
