#	$Id: Makefile,v 1.14 1998/09/05 03:30:54 jdp Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN8=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
