# $NetBSD: Makefile,v 1.10 2011/05/26 12:56:32 joerg Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/include
TESTS_SUBDIRS=	machine sys

TESTS_C=	t_bitstring
TESTS_C+=	t_errno
TESTS_C+=	t_glob
TESTS_C+=	t_inttypes
TESTS_C+=	t_limits
TESTS_C+=	t_netdb
TESTS_C+=	t_paths
TESTS_C+=	t_stdint

FILESDIR=	${TESTSDIR}
FILES=		d_bitstring_27.out
FILES+=		d_bitstring_32.out
FILES+=		d_bitstring_49.out
FILES+=		d_bitstring_64.out
FILES+=		d_bitstring_67.out
FILES+=		d_bitstring_8.out

CWARNFLAGS.clang+=	-Wno-uninitialized

.include <bsd.test.mk>
