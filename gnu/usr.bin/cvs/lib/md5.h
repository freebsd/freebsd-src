#ifndef MD5_H
#define MD5_H

#if SIZEOF_LONG == 4
typedef unsigned long uint32;
#else
#if SIZEOF_INT == 4
typedef unsigned int uint32;
#else
Congratulations!  You get to rewrite this code so that it does not require
a 32-bit integer type!  (Or maybe you just need to reconfigure.)
#endif
#endif

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
};

void MD5Init PROTO((struct MD5Context *context));
void MD5Update PROTO((struct MD5Context *context, unsigned char const *buf, unsigned len));
void MD5Final PROTO((unsigned char digest[16], struct MD5Context *context));
void MD5Transform PROTO((uint32 buf[4], uint32 const in[16]));

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#endif /* !MD5_H */
