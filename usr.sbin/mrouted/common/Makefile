#	$Id: Makefile,v 1.1 1994/09/08 02:51:32 wollman Exp $

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
