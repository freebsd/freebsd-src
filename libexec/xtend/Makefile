# Makefile for xtend (Stark) 10/30/93
# $Id$

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+=-I. -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
LDADD+= -lutil

MAN8=	xtend.8

.include <bsd.prog.mk>
