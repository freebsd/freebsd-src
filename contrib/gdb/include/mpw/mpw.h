/* Mac MPW host-specific definitions. */

#ifndef	__INCLUDE_MPW_H
#define __INCLUDE_MPW_H

#ifndef MPW
#define MPW
#endif

/* MPW C is basically ANSI, but doesn't actually enable __STDC__,
   nor does it allow __STDC__ to be #defined. */

#ifndef ALMOST_STDC
#define ALMOST_STDC
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define HAVE_TIME_T_IN_TIME_H 1

#define HAVE_STDLIB_H 1

#define HAVE_ERRNO_H 1

#define HAVE_STDDEF_H 1

#define HAVE_STRING_H 1

#define HAVE_STDARG_H 1

#define HAVE_VPRINTF 1

#ifdef USE_MW_HEADERS

#include <unix.h>

#else

#include <fcntl.h>
#include <ioctl.h>
#include <sys/stat.h>

#define HAVE_FCNTL_H 1

#ifndef	O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

#ifndef fileno
#define fileno(fp) ((fp)->_file)
#endif

/* stdio.h does not define this if __STDC__, so define here. */

#ifdef __STDC__
FILE *fdopen(int fildes, const char *mode);
#endif

#endif /* USE_MW_HEADERS */

/* Add ersatz definitions, for systems that lack them.  */

#ifndef EIO
#define EIO 96
#endif
#ifndef ENOENT
#define ENOENT 97
#endif
#ifndef EACCES
#define EACCES 98
#endif
#ifndef ENOSYS
#define ENOSYS 99
#endif

#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

/* Binary files have different characteristics; for instance, no cr/nl
   translation. */

#define USE_BINARY_FOPEN

#include <spin.h>

#ifdef MPW_C
#undef  __PTR_TO_INT
#define __PTR_TO_INT(P) ((int)(P))
#undef __INT_TO_PTR
#define __INT_TO_PTR(P) ((char *)(P))
#endif /* MPW_C */

#define NO_FCNTL

int fstat ();

FILE *mpw_fopen ();
int mpw_fseek ();
int mpw_fread ();
int mpw_fwrite ();
int mpw_access ();
int mpw_open ();
int mpw_creat ();
void mpw_abort (void);

/* Map these standard functions to improved versions in libiberty. */

#define fopen mpw_fopen
#define fseek mpw_fseek
#define fread mpw_fread
#define fwrite mpw_fwrite
#define open mpw_open
#define access mpw_access
#define creat mpw_creat
#define abort mpw_abort

#define POSIX_UTIME

#define LOSING_TOTALLY

/* Define this so that files will be closed before being unlinked. */

#define CLOSE_BEFORE_UNLINK

#endif /* __INCLUDE_MPW_H */
