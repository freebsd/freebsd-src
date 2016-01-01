# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/geom/class

TESTS_SUBDIRS+=	concat
TESTS_SUBDIRS+=	eli
TESTS_SUBDIRS+=	gate
# XXX: might not work due to geom(4) changes; more investigation's needed
#TESTS_SUBDIRS+=	gpt
TESTS_SUBDIRS+=	mirror
TESTS_SUBDIRS+=	nop
TESTS_SUBDIRS+=	raid3
TESTS_SUBDIRS+=	shsec
TESTS_SUBDIRS+=	stripe
TESTS_SUBDIRS+=	uzip

BINDIR=		${TESTSDIR}

FILES+=		geom_subr.sh

.include <bsd.test.mk>
