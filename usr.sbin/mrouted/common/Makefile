#	$Id: Makefile,v 1.4 1997/06/29 06:03:38 pst Exp $

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
