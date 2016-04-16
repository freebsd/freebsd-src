# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/sys/geom

.if ${MACHINE_CPUARCH} != "mips"
TESTS_SUBDIRS+=	class
.endif

.include <bsd.test.mk>
