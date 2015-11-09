# $FreeBSD$

TESTGROUP= ${.CURDIR:H:T}/${.CURDIR:T}
TESTSRC= ${.CURDIR:H:H:H:H:H}/contrib/opensolaris/cmd/dtrace/test/tst/${TESTGROUP}
TESTSDIR= ${TESTSBASE}/cddl/usr.sbin/dtrace/${TESTGROUP}

FILESGROUPS+=	FILES ${TESTGROUP} ${TESTGROUP}EXE

${TESTGROUP}= ${TESTFILES}
${TESTGROUP}EXE= ${TESTEXES}
${TESTGROUP}EXEMODE= 0555

TESTWRAPPER=	t_dtrace_contrib
ATF_TESTS_SH+=	${TESTWRAPPER}
TEST_METADATA.t_dtrace_contrib+= required_files="/usr/local/bin/ksh"
TEST_METADATA.t_dtrace_contrib+= required_user="root"

GENTEST?=	${.CURDIR:H:H}/tools/gentest.sh
EXCLUDE=	${.CURDIR:H:H}/tools/exclude.sh
${TESTWRAPPER}.sh: ${GENTEST} ${EXCLUDE} ${${TESTGROUP}}
	sh ${GENTEST} -e ${EXCLUDE} ${TESTGROUP} ${${TESTGROUP}:S/ */ /} > ${.TARGET}

CLEANFILES+=	${TESTWRAPPER}.sh

.PATH:	${TESTSRC}

PROGS=		${CFILES:T:S/.c$/.exe/g}
.for prog in ${PROGS}
SRCS.${prog}+= ${prog:S/.exe$/.c/}

.if exists(${prog:S/^tst.//:S/.exe$/.d/})
SRCS.${prog}+=	${prog:S/^tst.//:S/.exe$/.d/}
.endif
.endfor

BINDIR=		${TESTSDIR}
MAN=

# Some tests depend on the internals of their corresponding test programs,
# so make sure the optimizer doesn't interfere with them.
CFLAGS+=	-O0

# Test programs shouldn't be stripped; else we generally can't use the PID
# provider.
DEBUG_FLAGS=	-g
STRIP=

.include <bsd.test.mk>
