# $FreeBSD: src/usr.sbin/mrouted/common/Makefile,v 1.5 1999/08/28 01:17:11 peter Exp $

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
