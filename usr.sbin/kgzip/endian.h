/*
 * $FreeBSD$
 */

#include <sys/types.h>
#include <machine/endian.h>

#define	bswap16(x) (uint16_t) \
	((x >> 8) | (x << 8))

#define	bswap32(x) (uint32_t) \
	((x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24))

#define	bswap64(x) (uint64_t) \
	((x >> 56) | ((x >> 40) & 0xff00) | ((x >> 24) & 0xff0000) | \
	((x >> 8) & 0xff000000) | ((x << 8) & ((uint64_t)0xff << 32)) | \
	((x << 24) & ((uint64_t)0xff << 40)) | \
	((x << 40) & ((uint64_t)0xff << 48)) | ((x << 56)))

/*
 * Host to big endian, host to little endian, big endian to host, and little
 * endian to host byte order functions as detailed in byteorder(9).
 */
#if _BYTE_ORDER == _LITTLE_ENDIAN
#define	HTOBE16(x)	bswap16((uint16_t)(x))
#define	HTOBE32(x)	bswap32((uint32_t)(x))
#define	HTOBE64(x)	bswap64((uint64_t)(x))
#define	HTOLE16(x)	((uint16_t)(x))
#define	HTOLE32(x)	((uint32_t)(x))
#define	HTOLE64(x)	((uint64_t)(x))

#define	BE16TOH(x)	bswap16((uint16_t)(x))
#define	BE32TOH(x)	bswap32((uint32_t)(x))
#define	BE64TOH(x)	bswap64((uint64_t)(x))
#define	LE16TOH(x)	((uint16_t)(x))
#define	LE32TOH(x)	((uint32_t)(x))
#define	LE64TOH(x)	((uint64_t)(x))
#else /* _BYTE_ORDER != _LITTLE_ENDIAN */
#define	HTOBE16(x)	((uint16_t)(x))
#define	HTOBE32(x)	((uint32_t)(x))
#define	HTOBE64(x)	((uint64_t)(x))
#define	HTOLE16(x)	bswap16((uint16_t)(x))
#define	HTOLE32(x)	bswap32((uint32_t)(x))
#define	HTOLE64(x)	bswap64((uint64_t)(x))

#define	BE16TOH(x)	((uint16_t)(x))
#define	BE32TOH(x)	((uint32_t)(x))
#define	BE64TOH(x)	((uint64_t)(x))
#define	LE16TOH(x)	bswap16((uint16_t)(x))
#define	LE32TOH(x)	bswap32((uint32_t)(x))
#define	LE64TOH(x)	bswap64((uint64_t)(x))
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */
