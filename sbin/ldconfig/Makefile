#	$Id: Makefile,v 1.11 1997/06/29 21:40:54 bde Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR}
MAN8=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
