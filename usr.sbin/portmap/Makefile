#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.4.10.1 1999/05/05 18:36:25 obrien Exp $

PROG=	portmap
MAN8=	portmap.8
SRCS=	portmap.c from_local.c pmap_check.c
SUBDIR= pmap_set pmap_dump

CFLAGS+=-DCHECK_PORT -DHOSTS_ACCESS 
DPADD=	${LIBWRAP}
LDADD=	-lwrap

.include <bsd.prog.mk>
