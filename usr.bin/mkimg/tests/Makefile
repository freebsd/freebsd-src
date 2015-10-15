# $FreeBSD$

ATF_TESTS_SH=	mkimg

SOURCES!=	cd ${.CURDIR}; echo *.uu
FILES=		${SOURCES:S,.gz.uu,,g}
FILESDIR=	${TESTSDIR}

${FILES}:
	uudecode -p ${.CURDIR}/${.TARGET}.gz.uu | gunzip -c > ${.TARGET}

CLEANFILES+=	${FILES}

rebase:
	(cd ${.CURDIR}; atf-sh mkimg.sh rebase)

.include <bsd.test.mk>
