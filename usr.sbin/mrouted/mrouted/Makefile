# $FreeBSD$

PROG=	mrouted

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
YFLAGS=

SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c \
	ipip.c icmp.c vers.c igmp.c inet.c kern.c
CLEANFILES+=	vers.c
MAN=	mrouted.8

vers.c:	$S/VERSION
	rm -f vers.c ; \
	sed 's/.*/char todaysversion[] = "&";/' $S/VERSION > vers.c

.include <bsd.prog.mk>
