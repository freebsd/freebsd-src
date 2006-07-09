# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/src
.PATH:	${CVSDIR}/man
.PATH:	${CVSDIR}

SCRIPTS=	cvsbug
MAN=		cvsbug.8

CLEANFILES+=    cvsbug

cvsbug: cvsbug.in
	version=`sed < ${CVSDIR}/configure \
	    -e '/^[     ]*VERSION=/!d' -e 's/.*=["'\'']\{0,1\}\([^"'\'']*\)["'\'']\{0,1\}/\1/' -e q`; \
	sed -e "s,@VERSION@,$${version}-FreeBSD,g" \
	    -e "s,@MKTEMP@,/usr/bin/mktemp,g" \
	    -e "s,@PACKAGE_BUGREPORT@,bug-cvs@gnu.org,g" \
	    -e "s,@SENDMAIL@,/usr/sbin/sendmail,g" \
	    -e "s,@MKTEMP_FUNCTION@,," \
	    ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
