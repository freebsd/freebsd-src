#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: infinity.c,v 1.2 1998/11/14 19:31:02 christos Exp $");
#endif /* LIBC_SCCS and not lint */
__FBSDID("$FreeBSD$");

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a sparc */
const char __infinity[] = { 0x7f, (char)0xf0, 0, 0, 0, 0, 0, 0 };
