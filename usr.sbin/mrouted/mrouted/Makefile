# $FreeBSD$

S=	${.CURDIR}/..
.PATH: $S

PROG=	mrouted
MAN=	mrouted.8
SRCS=	config.c cfparse.y main.c route.c vif.c prune.c callout.c rsrr.c \
	ipip.c icmp.c vers.c igmp.c inet.c kern.c
CLEANFILES+=	vers.c

CFLAGS+= -I$S
YFLAGS=

vers.c:	$S/VERSION
	rm -f vers.c ; \
	sed 's/.*/char todaysversion[] = "&";/' $S/VERSION > vers.c

.include <bsd.prog.mk>
