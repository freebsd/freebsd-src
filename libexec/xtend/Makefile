# Makefile for xtend (Stark) 10/30/93
# $FreeBSD$

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
