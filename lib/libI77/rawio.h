#ifdef KR_headers
#ifndef __FreeBSD__
extern FILE *fdopen();
#endif
#else
#ifdef MSDOS
#include "io.h"
#define close _close
#define creat _creat
#define open _open
#define read _read
#define write _write
#endif
#ifdef __cplusplus
extern "C" {
#endif
#ifndef MSDOS
#ifdef OPEN_DECL
#ifndef __FreeBSD__
extern int creat(const char*,int), open(const char*,int);
#endif
#endif
#ifndef __FreeBSD__
extern int close(int);
extern int read(int,void*,size_t), write(int,void*,size_t);
extern int unlink(const char*);
#endif
#ifndef _POSIX_SOURCE
#ifndef NON_UNIX_STDIO
#ifndef __FreeBSD__
extern FILE *fdopen(int, const char*);
#endif
#endif
#endif
#endif

extern char *mktemp(char*);

#ifdef __cplusplus
	}
#endif
#endif

#include "fcntl.h"

#ifndef O_WRONLY
#define O_RDONLY 0
#define O_WRONLY 1
#endif
