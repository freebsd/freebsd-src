#	$Id: Makefile,v 1.1.1.1.2.1 1997/08/22 10:24:36 peter Exp $

PROG=	natd
SRCS=	natd.c icmp.c
CFLAGS+=-Wall
LDADD=	-lalias
DPADD=	${LIBALIAS}
MAN8=	natd.8

.include <bsd.prog.mk>
