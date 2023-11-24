
.include <bsd.own.mk>

BINDIR=		${TESTSDIR}

NETBSD_ATF_TESTS_C=	fileactions_test
NETBSD_ATF_TESTS_C+=	spawn_test
NETBSD_ATF_TESTS_C+=	spawnattr_test

PROGS=		h_fileactions
PROGS+=		h_spawn
PROGS+=		h_spawnattr

SCRIPTS=	h_nonexec
SCRIPTS+=	h_zero

CLEANFILES+=	h_nonexec

.include "../../Makefile.netbsd-tests"

# The dd status=none option is non-standard. Only use it when this test succeeds
# rather than require dd to be a bootstrap tool.
DD_NOSTATUS!=(dd status=none count=0 2> /dev/null && echo status=none) || true
DD=dd ${DD_NOSTATUS}
h_zero:
	${DD} if=/dev/zero of=h_zero bs=1k count=2
	chmod a+x h_zero

CLEANFILES+=	h_zero

WARNS?=3

.include <bsd.test.mk>
