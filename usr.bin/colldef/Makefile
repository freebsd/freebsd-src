# $FreeBSD: src/usr.bin/colldef/Makefile,v 1.21.40.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	colldef
SRCS=	parse.y scan.l y.tab.h
LFLAGS=	-8 -i
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../lib/libc/locale
CFLAGS+=-DCOLLATE_DEBUG -DYY_NO_UNPUT
LDADD=	-ll
DPADD=	${LIBL}

.include <bsd.prog.mk>
