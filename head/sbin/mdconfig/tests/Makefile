# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sbin/mdconfig

TAP_TESTS_SH=			legacy_test
TAP_TESTS_SH_SED_legacy_test=   's,__PERL__,${TAP_PERL_INTERPRETER},g'
TEST_METADATA.legacy_test+=	required_programs="${TAP_PERL_INTERPRETER}"

FILESDIR=	${TESTSDIR}
FILES=		mdconfig.test
FILES+=		run.pl

.include <bsd.test.mk>
