/*
 * Written by Manuel Bouyer <bouyer@netbsd.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: bswap32.c,v 1.1 1997/10/09 15:42:33 bouyer Exp $";
static char *rcsid = "$NetBSD: bswap64.c,v 1.3 2009/03/16 05:59:21 cegger Exp $";
#endif

#include <sys/types.h>

#undef bswap32
#undef bswap64

u_int32_t bswap32(u_int32_t x);
u_int64_t bswap64(u_int64_t x);

u_int32_t
bswap32(u_int32_t x)
{
	return  ((x << 24) & 0xff000000 ) |
			((x <<  8) & 0x00ff0000 ) |
			((x >>  8) & 0x0000ff00 ) |
			((x >> 24) & 0x000000ff );
}

u_int64_t
bswap64(u_int64_t x)
{
#ifdef __LP64__
	/*
	 * Assume we have wide enough registers to do it without touching
	 * memory.
	 */
	return  ( (x << 56) & 0xff00000000000000UL ) |
		( (x << 40) & 0x00ff000000000000UL ) |
		( (x << 24) & 0x0000ff0000000000UL ) |
		( (x <<  8) & 0x000000ff00000000UL ) |
		( (x >>  8) & 0x00000000ff000000UL ) |
		( (x >> 24) & 0x0000000000ff0000UL ) |
		( (x >> 40) & 0x000000000000ff00UL ) |
		( (x >> 56) & 0x00000000000000ffUL );
#else
	/*
	 * Split the operation in two 32bit steps.
	 */
	u_int32_t tl, th;

	th = bswap32((u_int32_t)(x & 0x00000000ffffffffULL));
	tl = bswap32((u_int32_t)((x >> 32) & 0x00000000ffffffffULL));
	return ((u_int64_t)th << 32) | tl;
#endif
}
