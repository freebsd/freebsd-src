#	$Id: Makefile,v 1.10 1998/05/05 13:19:13 bde Exp $

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
YFLAGS=
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c
MAN8=	mrouted.8

.include <bsd.prog.mk>
