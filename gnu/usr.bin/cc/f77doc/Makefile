#	$Id: Makefile,v 1.1 1999/04/25 17:09:48 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f

INFO= g77

MAKEINFOFLAGS+= -I ${GCCDIR}/f

g77.info: g77.texi intdoc.texi

.include <bsd.info.mk>
