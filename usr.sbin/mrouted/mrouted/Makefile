#	$Id: Makefile,v 1.8 1998/03/07 09:49:11 bde Exp $

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c
MAN8=	mrouted.8

.include <bsd.prog.mk>
