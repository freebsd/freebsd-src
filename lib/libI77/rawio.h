#ifndef KR_headers
#ifdef MSDOS
#include "io.h"
#ifndef WATCOM
#define close _close
#define creat _creat
#define open _open
#define read _read
#define write _write
#endif /*WATCOM*/
#endif /*MSDOS*/
#ifdef __cplusplus
extern "C" {
#endif
#ifndef MSDOS
#ifdef OPEN_DECL
extern int creat(const char*,int), open(const char*,int);
#endif
extern int close(int);
extern int read(int,void*,size_t), write(int,void*,size_t);
extern int unlink(const char*);
#ifndef _POSIX_SOURCE
#ifndef NON_UNIX_STDIO
extern FILE *fdopen(int, const char*);
#endif
#endif
#endif /*KR_HEADERS*/

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
