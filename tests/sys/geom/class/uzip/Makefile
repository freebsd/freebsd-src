#
# $FreeBSD$
#
# Regression test for geom_uzip.
#

TESTSDIR=	${TESTSBASE}/sys/geom/class/${.CURDIR:T}

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

FILES+=		conf.sh
FILESNAME_conf.sh=	conf.sh

FILES+=		${UZIMAGE}
FILESDIR=	${TESTSDIR}

FILESGROUPS=	FILES etalon
etalon+=	etalon/etalon.txt
etalonDIR=	${TESTSDIR}/etalon

TAP_TESTS_SH+=	1_test

.for t in ${TAP_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
