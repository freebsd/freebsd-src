/*
 * $FreeBSD$
 */

#include <sys/param.h>

#if __FreeBSD_version >= 500034
#include <sys/endian.h>
#else
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
#define	htobe16(x)	bswap16((uint16_t)(x))
#define	htobe32(x)	bswap32((uint32_t)(x))
#define	htobe64(x)	bswap64((uint64_t)(x))
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#define	htole64(x)	((uint64_t)(x))

#define	be16toh(x)	bswap16((uint16_t)(x))
#define	be32toh(x)	bswap32((uint32_t)(x))
#define	be64toh(x)	bswap64((uint64_t)(x))
#define	le16toh(x)	((uint16_t)(x))
#define	le32toh(x)	((uint32_t)(x))
#define	le64toh(x)	((uint64_t)(x))
#else /* _BYTE_ORDER != _LITTLE_ENDIAN */
#define	htobe16(x)	((uint16_t)(x))
#define	htobe32(x)	((uint32_t)(x))
#define	htobe64(x)	((uint64_t)(x))
#define	htole16(x)	bswap16((uint16_t)(x))
#define	htole32(x)	bswap32((uint32_t)(x))
#define	htole64(x)	bswap64((uint64_t)(x))

#define	be16toh(x)	((uint16_t)(x))
#define	be32toh(x)	((uint32_t)(x))
#define	be64toh(x)	((uint64_t)(x))
#define	le16toh(x)	bswap16((uint16_t)(x))
#define	le32toh(x)	bswap32((uint32_t)(x))
#define	le64toh(x)	bswap64((uint64_t)(x))
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */
#endif
