# $FreeBSD$

S=	${.CURDIR}/..
.PATH: $S

LIB=	mrouted
NOPROFILE= yes
NOPIC=	yes
NOMAN=	#true
SRCS=	igmp.c inet.c kern.c

CFLAGS+= -I$S

# nothing to install
install:

.include <bsd.lib.mk>
