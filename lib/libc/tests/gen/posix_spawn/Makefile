# $FreeBSD$

OBJTOP=		${.OBJDIR:H:H:H:H:H}
SRCTOP=		${.CURDIR:H:H:H:H:H}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libc/gen/${.CURDIR:T}

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/gen/posix_spawn

BINDIR=		${TESTSDIR}

NETBSD_ATF_TESTS_C=	fileactions_test
NETBSD_ATF_TESTS_C+=	spawn_test
NETBSD_ATF_TESTS_C+=	spawnattr_test

PROGS=		h_fileactions
PROGS+=		h_spawn
PROGS+=		h_spawnattr

SCRIPTS=	h_nonexec
SCRIPTS+=	h_zero

.include "../../Makefile.netbsd-tests"

h_zero:
	dd if=/dev/zero of=h_zero bs=1k count=2
	chmod a+x h_zero

CLEANFILES+=	h_zero

WARNS?=3

.include <bsd.test.mk>
