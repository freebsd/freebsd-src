# Makefile for xtend (Stark) 10/30/93
# $Id: Makefile,v 1.2 1995/04/18 01:53:56 jkh Exp $

BINMODE=	555

PROG=	xtend
SRCS=	xtend.c status.c packet.c user.c
CFLAGS+= -DXTENUNAME=\"xten\" -DXTENGNAME=\"xten\"
LDADD+= -lutil

MAN8=	xtend.8

.include <bsd.prog.mk>
