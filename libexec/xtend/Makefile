# Makefile for xtend (Stark) 10/30/93
# $FreeBSD: src/libexec/xtend/Makefile,v 1.7 1999/08/28 00:10:29 peter Exp $

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
DPADD=	${LIBUTIL}
LDADD=	-lutil

MAN8=	xtend.8

.include <bsd.prog.mk>
