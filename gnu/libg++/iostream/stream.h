#ifndef _COMPAT_STREAM_H
#define _COMPAT_STREAM_H

// Compatibility with old library.

#define _STREAM_COMPAT
#include <iostream.h>

extern char* form(const char*, ...);

extern char* dec(long, int=0);
extern char* dec(int, int=0);
extern char* dec(unsigned long, int=0);
extern char* dec(unsigned int, int=0);

extern char* hex(long, int=0);
extern char* hex(int, int=0);
extern char* hex(unsigned long, int=0);
extern char* hex(unsigned int, int=0);

extern char* oct(long, int=0);
extern char* oct(int, int=0);
extern char* oct(unsigned long, int=0);
extern char* oct(unsigned int, int=0);

char*        chr(char ch, int width = 0);
char*        str(const char* s, int width = 0);

inline istream& WS(istream& str) { return ws(str); }

#endif /* !_COMPAT_STREAM_H */
