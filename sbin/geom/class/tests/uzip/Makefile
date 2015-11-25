#
# $FreeBSD$
#
# Regression test for geom_uzip.
#

IMAGE=		1.img
ZIMAGE=		${IMAGE}.uzip
UZIMAGE=	${ZIMAGE}.uue

CLEANFILES+=	${IMAGE} ${UZIMAGE} ${ZIMAGE}

${IMAGE}:
	makefs -s 1048576 ${.TARGET} ${.CURDIR}/etalon

${ZIMAGE}: ${IMAGE}
	mkuzip -o ${.TARGET} ${.ALLSRC}

${UZIMAGE}: ${IMAGE} ${ZIMAGE}
	printf "#\n# $$" >${.TARGET}
	printf "FreeBSD$$\n#\n\n" >> ${.TARGET}
	uuencode ${ZIMAGE} ${ZIMAGE} >>${.TARGET}

FILES+=		${UZIMAGE}
FILESDIR=	${TESTSDIR}

FILESGROUPS=	FILES etalon
etalon+=	etalon/etalon.txt
etalonDIR=	${TESTSDIR}/etalon

PLAIN_TESTS_SH+=	1_test
PLAIN_TESTS_SH+=	2_test

.include <bsd.test.mk>
