# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

SRCDIR= ${CVSDIR}/doc

INFO=	cvs cvsclient
INFOSECTION= "Programming & development tools."
INFOENTRY_cvs= "* CVS: (cvs).			CVS Reference Manual."
INFOENTRY_cvsclient= "* CVS-CLIENT: (cvsclient).	CVS client/server Reference Manual."


cvs.info cvsclient.info:	CVSvn.texi

CLEANFILES+= CVSvn.texi

CVSvn.texi: ${CVSDIR}/src/version.c
	echo "@set CVSVN `sed < ${CVSDIR}/src/version.c \
		    -e '/version_string/!d' \
		    -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q`" > $@

.include <bsd.info.mk>
