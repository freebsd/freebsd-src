# Makefile for xtend (Stark) 10/30/93
# $FreeBSD: src/libexec/xtend/Makefile,v 1.9 2001/03/26 14:22:12 ru Exp $

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
DPADD=	${LIBUTIL}
LDADD=	-lutil

MAN=	xtend.8

.include <bsd.prog.mk>
