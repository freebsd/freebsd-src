#	$Id: Makefile,v 1.11 1999/01/20 07:55:57 fenner Exp $

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
YFLAGS=

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c \
	ipip.c icmp.c vers.c igmp.c inet.c kern.c
CLEANFILES+=	vers.c
MAN8=	mrouted.8

vers.c:	$S/VERSION
	rm -f vers.c ; \
	sed 's/.*/char todaysversion[] = "&";/' $S/VERSION > vers.c

.include <bsd.prog.mk>
