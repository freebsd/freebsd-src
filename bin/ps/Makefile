# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c lomac.c
#
# To support "lazy" ps for non root/wheel users
# add -DLAZY_PS to the cflags.  This helps
# keep ps from being an unnecessary load
# on large systems.
#
CFLAGS+=-DLAZY_PS
NO_WERROR=yes
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm
#BINGRP=	kmem
#BINMODE=2555

WFORMAT?=	0

.include <bsd.prog.mk>
