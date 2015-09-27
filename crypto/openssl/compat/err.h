/*
 * Public domain
 * err.h compatibility shim
 */

#ifdef HAVE_ERR_H

#include_next <err.h>

#else

#ifndef LIBCRYPTOCOMPAT_ERR_H
#define LIBCRYPTOCOMPAT_ERR_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define err(exitcode, format, ...) \
  errx(exitcode, format ": %s", ## __VA_ARGS__, strerror(errno))

#define errx(exitcode, format, ...) \
  do { warnx(format, ## __VA_ARGS__); exit(exitcode); } while (0)

#define warn(format, ...) \
  warnx(format ": %s", ## __VA_ARGS__, strerror(errno))

#define warnx(format, ...) \
  fprintf(stderr, format "\n", ## __VA_ARGS__)

#endif

#endif
