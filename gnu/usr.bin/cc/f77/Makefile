#	$Id: Makefile,v 1.7 1999/04/18 09:42:56 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f ${GCCDIR}

PROG=	f77
SRCS=	gcc.c g77spec.c version.c

CFLAGS+=	-DLANG_SPECIFIC_DRIVER

DPADD=	${LIBCC_DRV} 
LDADD=	${LIBCC_DRV}

f77.1: g77.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
