# $FreeBSD$

PACKAGE=	tests

ATF_TESTS_SH=	pw_etcdir \
		pw_lock \
		pw_config \
		pw_groupadd \
		pw_groupdel \
		pw_groupmod \
		pw_useradd \
		pw_userdel \
		pw_usermod \
		pw_usernext

.for tp in ${ATF_TESTS_SH}
TEST_METADATA.${tp}+=	required_user="root"
.endfor

${PACKAGE}FILES+=	group
${PACKAGE}FILES+=	helper_functions.shin
${PACKAGE}FILES+=	master.passwd
${PACKAGE}FILES+=	pw.conf
${PACKAGE}FILES+=	pw-modified.conf

.include <bsd.test.mk>
