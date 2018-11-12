#ifdef __cplusplus
#error "Do not use C++.  See the INSTALL file."
#endif

#ifndef MANDOC_CONFIG_H
#define MANDOC_CONFIG_H

#if defined(__linux__) || defined(__MINT__)
#define _GNU_SOURCE	/* See test-*.c what needs this. */
#endif

#include <sys/types.h>
#include <stdio.h>

#define MAN_CONF_FILE "/etc/man.conf"
#define HAVE_DIRENT_NAMLEN 1
#define HAVE_ERR 1
#define HAVE_FTS 1
#define HAVE_GETLINE 1
#define HAVE_GETSUBOPT 1
#define HAVE_ISBLANK 1
#define HAVE_MKDTEMP 1
#define HAVE_MMAP 1
#define HAVE_PLEDGE 0
#define HAVE_PROGNAME 1
#define HAVE_REALLOCARRAY 1
#define HAVE_REWB_BSD 0
#define HAVE_REWB_SYSV 0
#define HAVE_STRCASESTR 1
#define HAVE_STRINGLIST 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1
#define HAVE_STRPTIME 1
#define HAVE_STRSEP 1
#define HAVE_STRTONUM 1
#define HAVE_VASPRINTF 1
#define HAVE_WCHAR 1
#define HAVE_SQLITE3 1
#define HAVE_SQLITE3_ERRSTR 0
#define HAVE_OHASH 1
#define HAVE_MANPATH 1

#define BINM_APROPOS "apropos"
#define BINM_MAKEWHATIS "makewhatis"
#define BINM_MAN "man"
#define BINM_SOELIM "soelim"
#define BINM_WHATIS "whatis"

extern	ssize_t	  getline(char **, size_t *, FILE *);
extern	const char *sqlite3_errstr(int);

#endif /* MANDOC_CONFIG_H */
