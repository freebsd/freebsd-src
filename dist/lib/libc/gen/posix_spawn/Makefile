# $NetBSD: Makefile,v 1.2 2012/02/14 00:13:54 martin Exp $

NOMAN=		# defined
WARNS=4

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/gen/posix_spawn

TESTS_C=	t_spawn
TESTS_C+=	t_fileactions
TESTS_C+=	t_spawnattr

BINDIR=		${TESTSDIR}
SCRIPTSDIR=	${TESTSDIR}

PROGS=		h_fileactions
PROGS+=		h_spawn
PROGS+=		h_spawnattr
SCRIPTS=	h_nonexec h_zero

h_zero:
	dd if=/dev/zero of=h_zero bs=1k count=2
	chmod a+x h_zero

CLEANFILES+=	h_nonexec h_zero

.include <bsd.test.mk>
