WORLDPACKAGE=	tests
SUBPACKAGES=	dbg

PKG_SETS=	tests

# Nearly all the tests require atf to run.
PKG_DEPS.tests+=	atf

# Quite a few tests require flua.
PKG_DEPS.tests+=	flua

# Some tests need GoogleTest
PKG_DEPS.tests+=	googletest

# The test framework requires Kyua.
PKG_DEPS.tests+=	kyua

# Since the purpose of the tests is to test the base system, the base
# system must be installed.
PKG_DEPS.tests+=	set-base

.include <bsd.pkg.mk>
