#
# $Id: Makefile,v 1.14 1999/04/04 16:36:32 obrien Exp $
#

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG =	cc
MAN1 =  gcc.1
SRCS =	gcc.c

LINKS=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS= gcc.1 cc.1 gcc.1 c++.1 gcc.1 g++.1 gcc.1 CC.1

DPADD+=	${LIBCC_DRV} 
LDADD+=	${LIBCC_DRV}

.include <bsd.prog.mk>
