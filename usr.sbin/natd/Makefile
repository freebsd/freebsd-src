#	$Id: Makefile,v 1.1.1.1.2.2 1998/02/20 15:42:04 brian Exp $

PROG=	natd
SRCS=	natd.c icmp.c
CFLAGS+=-Wall
LDADD=	-lalias
DPADD=	${LIBALIAS}
MAN8=	natd.8

.include <bsd.prog.mk>
