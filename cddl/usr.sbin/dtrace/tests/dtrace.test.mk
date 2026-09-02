
TESTGROUP= ${.CURDIR:H:T}/${.CURDIR:T}
TESTBASE= ${SRCTOP}/cddl/contrib/opensolaris/cmd/dtrace/test/tst
TESTSRC= ${TESTBASE}/${TESTGROUP}
TESTSDIR= ${TESTSBASE}/cddl/usr.sbin/dtrace/${TESTGROUP}

FILESGROUPS+=	${TESTGROUP}EXE

${TESTGROUP}EXE= ${TESTEXES}
${TESTGROUP}EXEMODE= 0555
${TESTGROUP}EXEPACKAGE=	${PACKAGE}

TESTWRAPPER=	t_dtrace_contrib
ATF_TESTS_SH+=	${TESTWRAPPER}
TEST_METADATA.t_dtrace_contrib+= required_programs="ksh jq perl xmllint"
TEST_METADATA.t_dtrace_contrib+= required_user="root"

GENTEST?=	${.CURDIR:H:H}/tools/gentest.sh
EXCLUDE=	${.CURDIR:H:H}/tools/exclude.sh
${TESTWRAPPER}.sh: ${GENTEST} ${EXCLUDE} ${TFILES}
	env TESTBASE=${TESTBASE:Q} \
	    sh ${GENTEST} -e ${EXCLUDE} ${TESTGROUP} ${TFILES:S/ */ /} > ${.TARGET}

CLEANFILES+=	${TESTWRAPPER}.sh

.PATH:	${TESTSRC}

# Only precompile C files if they don't have a
# corresponding D source. This prevents compiling
# the D source for the host architecture in cross
# builds.
.for prog in ${CFILES:T:S/.c$/.exe/g}
.if !exists(${prog:S/^tst.//:S/.exe$/.d/})
PROGS+=		${prog}
SRCS.${prog}+=	${prog:S/.exe$/.c/}
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
