# $Id: Makefile,v 1.4 1996/08/21 00:35:53 peter Exp $

BINDIR?=	${INFODIR}

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
