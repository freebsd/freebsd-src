# $FreeBSD$

TESTSRC=	${.CURDIR}/../../../contrib/netbsd-tests/usr.sbin/useradd
.PATH: ${TESTSRC}

TESTSDIR=	${TESTSBASE}/usr.sbin/pw

ATF_TESTS_SH=	pw_etcdir \
		pw_lock \
		pw_config \
		pw_groupdel \
		pw_groupmod \
		pw_useradd \
		pw_userdel \
		pw_usermod \
		pw_usernext

.for tp in ${ATF_TESTS_SH}
TEST_METADATA.${tp}+=	required_user="root"
.endfor

FILES=		group helper_functions.shin master.passwd pw.conf \
		pw-modified.conf
FILESDIR=       ${TESTSDIR}

ATF_TESTS_SH+=	pw_test
# - user{add,del} does not exist on FreeBSD; use pw user{add,del} instead
# - The command passes on FreeBSD
ATF_TESTS_SH_SED_pw_test=	-e 's/useradd /pw useradd /'
ATF_TESTS_SH_SED_pw_test+=	-e 's/userdel /pw userdel /'
ATF_TESTS_SH_SED_pw_test+=	-e '/atf_expect_fail "PR bin\/39546"/d'
ATF_TESTS_SH_SRC_pw_test=	t_useradd.sh

.include <bsd.test.mk>
