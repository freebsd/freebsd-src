# $FreeBSD$

LIB=		mrouted
NOPROFILE=	yes
NOPIC=		yes

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+= -I$S

SRCS=	igmp.c inet.c kern.c
NOMAN=

# nothing to install
install:

.include <bsd.lib.mk>
