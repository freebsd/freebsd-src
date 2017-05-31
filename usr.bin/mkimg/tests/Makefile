# $FreeBSD$

.PATH:	${.CURDIR}

PACKAGE=	tests

ATF_TESTS_SH=	mkimg_test

SOURCES!=	cd ${.CURDIR}; echo *.uu
${PACKAGE}FILES+=		${SOURCES:S,.gz.uu,,g}

.for f in ${${PACKAGE}FILES}
$f: $f.gz.uu
	uudecode -p ${.ALLSRC} | gunzip -c > ${.TARGET}
.endfor

CLEANFILES+=	${${PACKAGE}FILES}}

rebase:
	(cd ${.CURDIR}; atf-sh mkimg.sh rebase)

.include <bsd.test.mk>
