# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

NETBSD_ATF_TESTS_C=

.if ${MK_OPENSSL} != "no"
NETBSD_ATF_TESTS_C+=	sha2_test
.endif

NETBSD_ATF_TESTS_SH=	hash_test

BINDIR=		${TESTSDIR}

PROGS+=		h_hash

FILESDIR=	${TESTSDIR}/data

FILES+=		data/md5test-in
FILES+=		data/md5test-out
FILES+=		data/sha1test-in
FILES+=		data/sha1test-out
FILES+=		data/sha1test2-out

LIBADD+=		md
LIBADD.sha2_test+=	crypto

CFLAGS.sha2_test+=	-I${SRCTOP}/crypto/openssh/openbsd-compat
CFLAGS.sha2_test+=	-I${SRCTOP}/crypto/openssh

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
