# $Id: Makefile,v 1.3 1996/08/20 23:55:34 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/doc

MAKEINFOFLAGS+=	-I ${CVSDIR}/doc

INFO=	cvs cvsclient

cvs.info cvsclient.info:	CVSvn.texi

CLEANFILES+= CVSvn.texi

CVSvn.texi: ${CVSDIR}/src/version.c
	echo "@set CVSVN `sed < ${CVSDIR}/src/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`" > $@

.include <bsd.info.mk>
