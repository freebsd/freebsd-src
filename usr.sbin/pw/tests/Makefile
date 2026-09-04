PACKAGE=	tests

BINDIR=		${TESTSDIR}

.PATH:		${SRCTOP}/usr.sbin/pw

PROGS+=		crypt
LIBADD.crypt+=	crypt

ATF_TESTS_C=	pw_unit_test
SRCS.pw_unit_test=	pw_unit_test.c \
			pw_utils.c strtounum.c
LIBADD.pw_unit_test=	util
CFLAGS.pw_unit_test=	-I${SRCTOP}/usr.sbin/pw

ATF_TESTS_SH=	pw_test\
		pw_etcdir_test \
		pw_lock_test \
		pw_config_test \
		pw_groupadd_test \
		pw_groupdel_test \
		pw_groupmod_test \
		pw_groupshow_test \
		pw_useradd_test \
		pw_userdel_test \
		pw_usermod_test \
		pw_usernext_test \
		pw_usershow_test

.for tp in ${ATF_TESTS_SH}
TEST_METADATA.${tp}+=	required_user="root"
.endfor

${PACKAGE}FILES+=	group
${PACKAGE}FILES+=	helper_functions.shin
${PACKAGE}FILES+=	master.passwd
${PACKAGE}FILES+=	pw.conf
${PACKAGE}FILES+=	pw-modified.conf

.include <bsd.test.mk>
