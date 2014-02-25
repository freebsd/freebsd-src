# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/bin/sh

TAP_TESTS_SH=	legacy_test
TAP_TESTS_SH_SED_legacy_test=	-e 's,__SH__,/bin/sh,g'

SUBDIR+=	builtins errors execution expansion parameters parser set-e

.include <tap.test.mk>
