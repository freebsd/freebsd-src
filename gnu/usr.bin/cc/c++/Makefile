#
# $Id: Makefile,v 1.10 1999/04/04 16:36:31 obrien Exp $
#

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG =	c++
SRCS =	gcc.c g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NOMAN=	1

CFLAGS+= -DLANG_SPECIFIC_DRIVER

DPADD+=	${LIBCC_DRV} 
LDADD+=	${LIBCC_DRV}

.include <bsd.prog.mk>
