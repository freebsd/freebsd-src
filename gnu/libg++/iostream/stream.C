#include <stdarg.h>
#include "ioprivate.h"
#include "stream.h"
#include "strstream.h"

static char Buffer[_G_BUFSIZ];
#define EndBuffer (Buffer+_G_BUFSIZ)
static char* next_chunk = Buffer; // Start of available part of Buffer.

char* form(const char* format, ...)
{
    int space_left = EndBuffer - next_chunk;
    // If less that 25% of the space is available start over.
    if (space_left < (_G_BUFSIZ>>2))
	next_chunk = Buffer;
    char* buf = next_chunk;

    strstreambuf stream(buf, EndBuffer-buf-1, buf);
    va_list ap;
    va_start(ap, format);
    int count = stream.vform(format, ap);
    va_end(ap);
    stream.sputc(0);
    next_chunk = buf + stream.pcount();
    return buf;
}

#define u_long unsigned long

static char* itoa(unsigned long i, int size, int neg, int base)
{
    // Conservative estimate: If base==2, might need 8 characters
    // for each input byte, but normally 3 is plenty.
    int needed = size ? size
	: (base >= 8 ? 3 : 8) * sizeof(unsigned long) + 2;
    int space_left = EndBuffer - next_chunk;
    if (space_left <= needed)
	next_chunk = Buffer; // start over.

    char* buf = next_chunk;

    register char* ptr = buf+needed+1;
    next_chunk = ptr;

    if (needed < (2+neg) || ptr > EndBuffer)
	return NULL;
    *--ptr = 0;
    
    if (i == 0)
	*--ptr = '0';
    while (i != 0 && ptr > buf) {
	int ch = i % base;
	i = i / base;
	if (ch >= 10)
	    ch += 'a' - 10;
	else
	    ch += '0';
	*--ptr = ch;
    }
    if (neg)
	*--ptr = '-';
    if (size == 0)
	return ptr;
    while (ptr > buf)
	*--ptr = ' ';
    return buf;
}

char* dec(long i, int len /* = 0 */)
{
    if (i >= 0) return itoa((unsigned long)i, len, 0, 10);
    else return itoa((unsigned long)(-i), len, 1, 10);
}
char* dec(int i, int len /* = 0 */)
{
    if (i >= 0) return itoa((unsigned long)i, len, 0, 10);
    else return itoa((unsigned long)(-i), len, 1, 10);
}
char* dec(unsigned long i, int len /* = 0 */)
{
    return itoa(i, len, 0, 10);
}
char* dec(unsigned int i, int len /* = 0 */)
{
    return itoa(i, len, 0, 10);
}

char* hex(long i, int len /* = 0 */)
{
    return itoa((unsigned long)i, len, 0, 16);
}
char* hex(int i, int len /* = 0 */)
{
    return itoa((unsigned long)i, len, 0, 16);
}
char* hex(unsigned long i, int len /* = 0 */)
{
    return itoa(i, len, 0, 16);
}
char* hex(unsigned int i, int len /* = 0 */)
{
    return itoa(i, len, 0, 16);
}

char* oct(long i, int len /* = 0 */)
{
    return itoa((unsigned long)i, len, 0, 8);
}
char* oct(int i, int len /* = 0 */)
{
    return itoa((unsigned long)i, len, 0, 8);
}
char* oct(unsigned long i, int len /* = 0 */)
{
    return itoa(i, len, 0, 8);
}
char* oct(unsigned int i, int len /* = 0 */)
{
    return itoa(i, len, 0, 8);
}
