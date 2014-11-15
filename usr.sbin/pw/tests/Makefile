# $FreeBSD$

TESTSRC=	${.CURDIR}/../../../contrib/netbsd-tests/usr.sbin/useradd
.PATH: ${TESTSRC}

TESTSDIR=	${TESTSBASE}/usr.sbin/pw

ATF_TESTS_SH=	pw_delete pw_lock pw_modify pw_etcdir

TEST_METADATA.pw_delete+=	required_user="root"
TEST_METADATA.pw_modify+=	required_user="root"

FILES=		group helper_functions.shin master.passwd
FILESDIR=       ${TESTSDIR}

ATF_TESTS_SH+=	pw_test
# - user{add,del} does not exist on FreeBSD; use pw user{add,del} instead
# - The command passes on FreeBSD
ATF_TESTS_SH_SED_pw_test=	-e 's/useradd /pw useradd /'
ATF_TESTS_SH_SED_pw_test+=	-e 's/userdel /pw userdel /'
ATF_TESTS_SH_SED_pw_test+=	-e '/atf_expect_fail "PR bin\/39546"/d'
ATF_TESTS_SH_SRC_pw_test=	t_useradd.sh

.include <bsd.test.mk>
