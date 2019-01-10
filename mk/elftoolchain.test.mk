# $Id$
#
# Rules for handling libtest based test suites.
#

.if !defined(TOP)
.error Make variable \"TOP\" has not been defined.
.endif

TEST_BASE=	$(TOP)/test/libtest
TEST_LIB=	$(TEST_BASE)/lib
TEST_DRIVER=	${TEST_BASE}/driver
TEST_DRIVER_MAIN=	$(TEST_DRIVER)/test_main.o

CFLAGS+=	-I$(TEST_LIB) -I${TEST_DRIVER}

MAKE_TEST_SCAFFOLDING?=	yes

.if exists(${.CURDIR}/../Makefile.tset)
.include "${.CURDIR}/../Makefile.tset"
.endif

.if defined(TEST_SRCS)
PROG=		tc_${.CURDIR:T:R}

_C_SRCS=	${TEST_SRCS:M*.c}
_M4_SRCS=	${TEST_SRCS:M*.m4}

SRCS=		${_C_SRCS} ${_M4_SRCS}	# See <bsd.prog.mk>
CLEANFILES+=	${_M4_SRCS:S/.m4$/.c/g} ${TEST_DATA}

${PROG}:	${TEST_DATA} ${TEST_LIB} ${TEST_DRIVER_MAIN}

.if defined(MAKE_TEST_SCAFFOLDING) && ${MAKE_TEST_SCAFFOLDING} == "yes"
_TC_SRC=	${.OBJDIR}/tc.c		# Test scaffolding.

SRCS+=		${_TC_SRC}
CLEANFILES+=	${_TC_SRC}

# Generate the scaffolding file "tc.c" from the test objects.
_TEST_OBJS=	${_C_SRCS:S/.c$/.o/g} ${_M4_SRCS:S/.m4$/.o/g}
_MAKE_SCAFFOLDING=	${TEST_BASE}/bin/make-test-scaffolding
${_TC_SRC}:	${_TEST_OBJS}
	${_MAKE_SCAFFOLDING} -o ${.TARGET} ${.ALLSRC}
.endif
.endif

LDADD+=		${TEST_DRIVER_MAIN} -L${TEST_LIB} -ltest

.include "${TOP}/mk/elftoolchain.prog.mk"
