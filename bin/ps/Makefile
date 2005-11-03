# $FreeBSD: src/bin/ps/Makefile,v 1.27 2005/01/27 13:42:40 delphij Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c

#
# To support "lazy" ps for non root/wheel users
# add -DLAZY_PS to the cflags.  This helps
# keep ps from being an unnecessary load
# on large systems.
#
CFLAGS+=-DLAZY_PS
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm

.include <bsd.prog.mk>
