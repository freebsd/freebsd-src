#ifndef MANDOC_CONFIG_H
#define MANDOC_CONFIG_H

#if defined(__linux__) || defined(__MINT__)
#define _GNU_SOURCE	/* See test-*.c what needs this. */
#endif

#include <sys/types.h>

#define VERSION "1.13.1"
#define HAVE_DIRENT_NAMLEN 1
#define HAVE_FGETLN 1
#define HAVE_FTS 1
#define HAVE_GETSUBOPT 1
#define HAVE_MMAP 1
#define HAVE_REALLOCARRAY 0
#define HAVE_STRCASESTR 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1
#define HAVE_STRPTIME 1
#define HAVE_STRSEP 1
#define HAVE_WCHAR 1
#define HAVE_SQLITE3 1
#define HAVE_SQLITE3_ERRSTR 0
#define HAVE_OHASH 1
#define HAVE_MANPATH 1

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

extern	void	 *reallocarray(void *, size_t, size_t);
extern	const char *sqlite3_errstr(int);

#endif /* MANDOC_CONFIG_H */
