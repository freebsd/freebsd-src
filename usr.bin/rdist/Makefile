#	@(#)Makefile	8.3 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/rdist/Makefile,v 1.6 1999/08/28 01:05:05 peter Exp $

PROG=	rdist
SRCS=	docmd.c expand.c gram.y lookup.c main.c rshrcmd.c server.c
CFLAGS+=-I${.CURDIR}
YFLAGS=

# To use the old method, which requires setuid-root and all the baggage and
# security holes that goes with it, uncomment:
# CFLAGS+= -DDIRECT_RCMD
# BINOWN=	root
# BINMODE=4555
# INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
