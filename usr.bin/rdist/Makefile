#	@(#)Makefile	8.3 (Berkeley) 7/19/93
# $FreeBSD$

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
