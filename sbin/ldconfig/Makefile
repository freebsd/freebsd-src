#	$Id: Makefile,v 1.13 1998/06/12 10:43:18 peter Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN8=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
