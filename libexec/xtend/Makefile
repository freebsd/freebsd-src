# Makefile for xtend (Stark) 10/30/93
# $FreeBSD: src/libexec/xtend/Makefile,v 1.6.2.1 1999/08/29 15:04:23 peter Exp $

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
DPADD=	${LIBUTIL}
LDADD=	-lutil

MAN8=	xtend.8

.include <bsd.prog.mk>
