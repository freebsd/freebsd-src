#ifndef _APPLE_ENDIAN_H
#define _APPLE_ENDIAN_H

/*
 * Shims to make Apple's endian headers and macros compatible
 * with <sys/endian.h> (which is awful).
 */

# include <libkern/OSByteOrder.h>

# define _LITTLE_ENDIAN 0x12345678
# define _BIG_ENDIAN 0x87654321

# ifdef __LITTLE_ENDIAN__
#  define _BYTE_ORDER _LITTLE_ENDIAN
# endif
# ifdef __BIG_ENDIAN__
#  define _BYTE_ORDER _BIG_ENDIAN
# endif

# define htole32(x)	OSSwapHostToLittleInt32(x)
# define le32toh(x)	OSSwapLittleToHostInt32(x)

# define htobe32(x)	OSSwapHostToBigInt32(x)
# define be32toh(x)	OSSwapBigToHostInt32(x)

#endif /* _APPLE_ENDIAN_H */
