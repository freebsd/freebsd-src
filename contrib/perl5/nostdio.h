/* This is an 1st attempt to stop other include files pulling 
   in real <stdio.h>.
   A more ambitious set of possible symbols can be found in
   sfio.h (inside an _cplusplus gard).
*/
#if !defined(_STDIO_H) && !defined(FILE) && !defined(_STDIO_INCLUDED) && !defined(__STDIO_LOADED)
#define _STDIO_H
#define _STDIO_INCLUDED
#define __STDIO_LOADED
struct _FILE;
#define FILE struct _FILE
#endif

#define _CANNOT "CANNOT"

#undef stdin
#undef stdout
#undef stderr
#undef getc
#undef putc
#undef clearerr
#undef fflush
#undef feof
#undef ferror
#undef fileno

