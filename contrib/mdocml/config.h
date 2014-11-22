#ifndef	MANDOC_CONFIG_H
#define	MANDOC_CONFIG_H

#if defined(__linux__) || defined(__MINT__)
# define _GNU_SOURCE /* getsubopt(), strcasestr(), strptime() */
#endif

#include <sys/types.h>
#include <stdio.h>

#define VERSION "1.13.1"
#define HAVE_FGETLN
#define HAVE_GETSUBOPT
#define HAVE_MMAP
#define HAVE_STRCASESTR
#define HAVE_STRLCAT
#define HAVE_STRLCPY
#define HAVE_STRPTIME
#define HAVE_STRSEP

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

#ifndef HAVE_FGETLN
extern	char	 *fgetln(FILE *, size_t *);
#endif
#ifndef HAVE_GETSUBOPT
extern	int	  getsubopt(char **, char * const *, char **);
extern	char	 *suboptarg;
#endif
#ifndef HAVE_REALLOCARRAY
extern	void	 *reallocarray(void *, size_t, size_t);
#endif
#ifndef HAVE_SQLITE3_ERRSTR
extern	const char *sqlite3_errstr(int);
#endif
#ifndef HAVE_STRCASESTR
extern	char	 *strcasestr(const char *, const char *);
#endif
#ifndef HAVE_STRLCAT
extern	size_t	  strlcat(char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
extern	size_t	  strlcpy(char *, const char *, size_t);
#endif
#ifndef HAVE_STRSEP
extern	char	 *strsep(char **, const char *);
#endif

#endif /* MANDOC_CONFIG_H */
