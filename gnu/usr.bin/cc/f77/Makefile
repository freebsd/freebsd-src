#
# $Id: Makefile,v 1.5 1999/04/18 09:22:56 obrien Exp $
#

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f ${GCCDIR}

PROG=	f77
SRCS=	gcc.c g77spec.c version.c

CFLAGS+=	-DLANG_SPECIFIC_DRIVER -DHAVE_CONFIG_H

DPADD=	${LIBCC_DRV} 
LDADD=	${LIBCC_DRV}

f77.1: g77.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
