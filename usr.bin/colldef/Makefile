# $FreeBSD: src/usr.bin/colldef/Makefile,v 1.21.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	colldef
SRCS=	parse.y scan.l y.tab.h
LFLAGS=	-8 -i
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale
CFLAGS+=-DCOLLATE_DEBUG -DYY_NO_UNPUT
LDADD=	-ll
DPADD=	${LIBL}

.include <bsd.prog.mk>
