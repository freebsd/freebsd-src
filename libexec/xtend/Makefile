# Makefile for xtend (Stark) 10/30/93
# $Id: Makefile,v 1.6 1997/02/22 14:22:41 peter Exp $

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
DPADD=	${LIBUTIL}
LDADD=	-lutil

MAN8=	xtend.8

.include <bsd.prog.mk>
