#
# $Id: Makefile,v 1.11 1999/03/05 04:54:59 obrien Exp $
#

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG =	c++
SRCS =	gcc.c g++spec.c
BINDIR=	/usr/bin
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NOMAN=	1

CFLAGS+= -DLANG_SPECIFIC_DRIVER

DPADD+=	${LIBCC_DRV} 
LDADD+=	${LIBCC_DRV}

.include <bsd.prog.mk>
