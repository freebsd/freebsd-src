.include <src.opts.mk>

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/geom/class

TESTS_SUBDIRS+=	concat
.if ${MK_OPENSSL} != "no"
TESTS_SUBDIRS+=	eli
.endif
TESTS_SUBDIRS+=	gate
TESTS_SUBDIRS+=	mirror
TESTS_SUBDIRS+=	multipath
TESTS_SUBDIRS+=	nop
TESTS_SUBDIRS+=	part
TESTS_SUBDIRS+=	raid3
TESTS_SUBDIRS+=	shsec
TESTS_SUBDIRS+=	stripe
TESTS_SUBDIRS+=	union
TESTS_SUBDIRS+=	uzip
TESTS_SUBDIRS+=	virstor

${PACKAGE}FILES+=	geom_subr.sh

.include <bsd.test.mk>
