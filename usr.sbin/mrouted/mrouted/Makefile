#	$Id: Makefile,v 1.7 1997/12/17 15:11:43 bde Exp $

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c
CLEANFILES+= y.tab.h
MAN8=	mrouted.8

.include <bsd.prog.mk>
