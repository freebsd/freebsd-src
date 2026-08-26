#ifndef LIBPKGCONF_PATH_H
#define LIBPKGCONF_PATH_H

#include <libpkgconf/libpkgconf.h>

PKGCONF_API bool pkgconf_path_trim_basename(pkgconf_buffer_t *buf);
PKGCONF_API const char *pkgconf_path_find_basename(const char *path);

#endif
