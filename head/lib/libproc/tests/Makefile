# $FreeBSD$

TESTSDIR=	${TESTSBASE}/lib/libproc

ATF_TESTS_C+=	proc_test

PROGS=		target_prog
SRCS_target_prog=	target_prog.c
BINDIR_target_prog=	${TESTSDIR}

LDADD+=	-lelf -lproc -lrtld_db -lutil
DPADD+=	${LIBELF} ${LIBPROC} ${LIBRTLD_DB} ${LIBUTIL}

# Ensure that symbols aren't stripped from the test program, as they're needed
# for testing symbol lookup.
STRIP=

MAN=
WARNS?=	6

.include <bsd.test.mk>
