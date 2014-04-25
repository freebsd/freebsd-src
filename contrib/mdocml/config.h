#ifndef	MANDOC_CONFIG_H
#define	MANDOC_CONFIG_H

#if defined(__linux__) || defined(__MINT__)
# define _GNU_SOURCE /* strptime(), getsubopt() */
#endif

#include <stdio.h>

#define VERSION "1.12.3"
#define HAVE_FGETLN
#define HAVE_STRPTIME
#define HAVE_GETSUBOPT
#define HAVE_STRLCAT
#define HAVE_MMAP
#define HAVE_STRLCPY

#include <sys/types.h>

#if !defined(__BEGIN_DECLS)
#  ifdef __cplusplus
#  define	__BEGIN_DECLS		extern "C" {
#  else
#  define	__BEGIN_DECLS
#  endif
#endif
#if !defined(__END_DECLS)
#  ifdef __cplusplus
#  define	__END_DECLS		}
#  else
#  define	__END_DECLS
#  endif
#endif

#ifndef HAVE_BETOH64
#  if defined(__APPLE__)
#    define betoh64(x) OSSwapBigToHostInt64(x)
#    define htobe64(x) OSSwapHostToBigInt64(x)
#  elif defined(__sun)
#    define betoh64(x) BE_64(x)
#    define htobe64(x) BE_64(x)
#  else
#    define betoh64(x) be64toh(x)
#  endif
#endif

#ifndef HAVE_STRLCAT
extern	size_t	  strlcat(char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
extern	size_t	  strlcpy(char *, const char *, size_t);
#endif
#ifndef HAVE_GETSUBOPT
extern	int	  getsubopt(char **, char * const *, char **);
extern	char	 *suboptarg;
#endif
#ifndef HAVE_FGETLN
extern	char	 *fgetln(FILE *, size_t *);
#endif

#endif /* MANDOC_CONFIG_H */
