#	$Id: Makefile,v 1.10 1998/05/05 13:19:13 bde Exp $

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
YFLAGS=
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c \
	ipip.c icmp.c vers.c
CLEANFILES+=	vers.c
MAN8=	mrouted.8

vers.c:	$S/VERSION
	rm -f vers.c ; \
	sed 's/.*/char todaysversion[] = "&";/' $S/VERSION > vers.c

.include <bsd.prog.mk>
