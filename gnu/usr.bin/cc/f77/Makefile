#
# $Id: Makefile,v 1.6 1999/03/05 04:55:03 obrien Exp $
#

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f ${GCCDIR}

PROG=	f77
SRCS=	gcc.c g77spec.c version.c
BINDIR=	/usr/bin

CFLAGS+=	-DLANG_SPECIFIC_DRIVER -DHAVE_CONFIG_H

DPADD=	${LIBCC_DRV} 
LDADD=	${LIBCC_DRV}

f77.1: g77.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
