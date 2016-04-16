# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

BINDIR=		${TESTSDIR}

PROGS=		h_db

FILESDIR=	${TESTSDIR}

FILES=		README

.if ${MACHINE_CPUARCH} != "mips"
# XXX: consumes a large amount of space in /tmp (200MB isn't enough)
NETBSD_ATF_TESTS_SH+=	db_test
ATF_TESTS_SH_SED_db_test=	-e 's,/bin/csh,/bin/cat,g'
.endif

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
