# $FreeBSD$

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

SRCDIR= ${CVSDIR}/doc

INFO=	cvs cvsclient
INFOSECTION= "Programming & development tools."
INFOENTRY_cvs= "* CVS: (cvs).			CVS Reference Manual."
INFOENTRY_cvsclient= "* CVS-CLIENT: (cvsclient).	CVS client/server Reference Manual."


cvs.info cvsclient.info:	CVSvn.texi

CLEANFILES+= CVSvn.texi

CVSvn.texi: ${CVSDIR}/configure
	echo "@set CVSVN `sed < ${CVSDIR}/configure \
		    -e '/^VERSION=/!d' \
		    -e 's/.*=\(.*\)/\1/' \
		    -e q`" > $@

.include <bsd.info.mk>
