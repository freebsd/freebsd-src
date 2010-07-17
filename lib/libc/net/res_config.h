/* $FreeBSD: src/lib/libc/net/res_config.h,v 1.9.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

#define	DEBUG	1	/* enable debugging code (needed for dig) */
#define	RESOLVSORT	/* allow sorting of addresses in gethostbyname */
#undef	SUNSECURITY	/* verify gethostbyaddr() calls - WE DONT NEED IT  */
#define MULTI_PTRS_ARE_ALIASES 1 /* fold multiple PTR records into aliases */
