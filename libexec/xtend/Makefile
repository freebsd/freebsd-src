# Makefile for xtend (Stark) 10/30/93
# $Id: Makefile,v 1.3 1996/09/04 22:22:54 bde Exp $

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
DPADD=	${LIBUTIL}
LDADD=	-lutil

MAN8=	xtend.8

.include <bsd.prog.mk>
