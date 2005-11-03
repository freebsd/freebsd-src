# $FreeBSD: src/gnu/usr.bin/cc/f77doc/Makefile,v 1.5 2003/07/01 14:46:26 ru Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f

INFO= g77

MAKEINFOFLAGS+= -I ${GCCDIR}/f -I ${GCCDIR}/doc/include
MAKEINFOFLAGS+= -D "srcdir src/contrib"

g77.info: g77.texi intdoc.texi ffe.texi invoke.texi news.texi bugs.texi \
    ../doc/include/gpl.texi ../doc/include/fdl.texi ../doc/include/funding.texi

.include <bsd.info.mk>
