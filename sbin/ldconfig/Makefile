#	$Id: Makefile,v 1.12 1998/06/01 13:58:19 peter Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN8=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
