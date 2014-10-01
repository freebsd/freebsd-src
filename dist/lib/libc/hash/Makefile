# $NetBSD: Makefile,v 1.2 2011/01/02 22:03:25 pgoyette Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/hash

TESTS_C+=	t_sha2

TESTS_SH+=	t_hash

BINDIR=		${TESTSDIR}
MKMAN=		no

PROGS+=		h_hash

FILESDIR=	${TESTSDIR}/data

FILES+=		data/md5test-in
FILES+=		data/md5test-out
FILES+=		data/sha1test-in
FILES+=		data/sha1test-out
FILES+=		data/sha1test2-out

.include <bsd.test.mk>
