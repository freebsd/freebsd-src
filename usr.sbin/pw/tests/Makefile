# $FreeBSD$

TESTSDIR=	${TESTSBASE}/usr.sbin/pw

BINDIR=		${TESTSDIR}

PROGS+=		crypt
LDADD+=		-lcrypt

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

FILES=		group helper_functions.shin master.passwd pw.conf \
		pw-modified.conf
FILESDIR=       ${TESTSDIR}

.include <bsd.test.mk>
