# $FreeBSD: src/usr.sbin/mrouted/common/Makefile,v 1.4.2.1 1999/08/29 15:44:05 peter Exp $

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
