#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/portmap/Makefile,v 1.7 1999/08/28 01:18:09 peter Exp $

PROG=	portmap
MAN8=	portmap.8
SRCS=	portmap.c from_local.c pmap_check.c
SUBDIR= pmap_set pmap_dump

CFLAGS+=-DCHECK_PORT -DHOSTS_ACCESS 
DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
