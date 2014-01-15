# $Id: elftoolchain.tet.mk 2844 2012-12-31 03:30:20Z jkoshy $
#
# Rules for handling TET based test suites.
#

.if !defined(TOP)
.error Make variable \"TOP\" has not been defined.
.endif

.include "${TOP}/mk/elftoolchain.tetvars.mk"

# Inform make(1) about the suffixes we use.
.SUFFIXES: .lsb32 .lsb64 .msb32 .msb64 .yaml

TS_ROOT?=	${.CURDIR:H}
TS_OBJROOT?=	${.OBJDIR:H}

TS_BASE=	${TOP}/test/tet

TET_LIBS=	${TET_ROOT}/lib/tet3
TET_OBJS=	${TET_LIBS}/tcm.o

CFLAGS+=	-I${TET_ROOT}/inc/tet3 -I${TS_ROOT}/common

# Bring in test-suite specific definitions, if any.
.if exists(${.CURDIR}/../Makefile.tset)
.include "${.CURDIR}/../Makefile.tset"
.endif

.if defined(TS_SRCS)
PROG=		tc_${.CURDIR:T:R}

_C_SRCS=	${TS_SRCS:M*.c}
_M4_SRCS=	${TS_SRCS:M*.m4}

SRCS=		${_C_SRCS} ${_M4_SRCS}	# See <bsd.prog.mk>.
CLEANFILES+=	${_M4_SRCS:S/.m4$/.c/g} ${TS_DATA}

${PROG}:	${TS_DATA} 

.if defined(GENERATE_TEST_SCAFFOLDING)
_TC_SRC=	${.OBJDIR}/tc.c				# Test driver.
_TC_SCN=	tet_scen				# Scenario file.

SRCS+=		${_TC_SRC}
CLEANFILES+=	${_TC_SRC} ${_TC_SCN}

# Generate the driver file "tc.c" from the objects comprising the test case.
_TS_OBJS=	${_C_SRCS:S/.c$/.o/g} ${_M4_SRCS:S/.m4$/.o/g}
_MUNGE_TS=	${TS_BASE}/bin/munge-ts
${_TC_SRC}:	${_TS_OBJS}
	${_MUNGE_TS} -o ${.TARGET} -p ${.CURDIR:H:T}/${.CURDIR:T:R}/${PROG} \
	-s ${_TC_SCN} ${.ALLSRC}
.endif
.endif

# M4->C translation.
M4FLAGS+=	-I${TS_ROOT}/common -I${TS_BASE}/common

.include "${TOP}/mk/elftoolchain.m4.mk"

LDADD+=		${TET_OBJS} -L${TET_LIBS} -lapi
CLEANFILES+=	tet_xres tet_captured

ELFTOOLCHAIN_AR=	${TOP}/ar/ar

.include "${TOP}/mk/elftoolchain.prog.mk"
