#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.5 1999/03/28 10:55:03 markm Exp $

PROG=	portmap
MAN8=	portmap.8
SRCS=	portmap.c from_local.c pmap_check.c
SUBDIR= pmap_set pmap_dump

CFLAGS+=-DCHECK_PORT -DHOSTS_ACCESS 
DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
