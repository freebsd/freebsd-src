#	$Id: Makefile,v 1.1.1.1 1997/06/22 04:19:08 brian Exp $

PROG=	natd
SRCS=	natd.c icmp.c
CFLAGS+=-Wall
LDADD=	-lalias
DPADD=	${LIBALIAS}
MAN8=	natd.8

.include <bsd.prog.mk>
