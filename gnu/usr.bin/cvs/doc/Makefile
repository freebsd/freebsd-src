# $Id: Makefile,v 1.2 1995/12/10 23:07:16 peter Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/doc

MAKEINFOFLAGS+=	-I ${CVSDIR}/doc

INFO=	cvs cvsclient

cvs.texinfo cvsclient.texi: CVSvn.texi

CLEANFILES+= CVSvn.texi

CVSvn.texi: ${CVSDIR}/src/version.c
	echo "@set CVSVN `sed < ${CVSDIR}/src/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`" > $@

.include <bsd.info.mk>
