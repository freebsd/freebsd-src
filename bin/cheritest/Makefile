# $FreeBSD$

PROG?=	cheritest
SRCS=	cheritest.c

.ifndef BOOTSTRAPPING
SRCS+=	cheritest_bounds_stack.c					\
	cheritest_ccall.c						\
	cheritest_fault.c						\
	cheritest_fd.c							\
	cheritest_libcheri.c						\
	cheritest_local.c						\
	cheritest_registers.c						\
	cheritest_sandbox.S						\
	cheritest_string.c						\
	cheritest_syscall.c						\
	cheritest_trusted_stack.c					\
	cheritest_util.c						\
	cheritest_var.c							\
	cheritest_vm.c							\
	cheritest_vm_swap.c						\
	cheritest_zlib.c
.endif

CHERITEST_DIR:=	${.PARSEDIR}

.ifdef CHERIABI_TESTS
.ifndef BOOTSTRAPPING
SRCS+= cheritest_cheriabi.c
.endif
CFLAGS+=	-DCHERIABI_TESTS
.endif

.ifdef CHERI_C_TESTS
CHERI_C_TESTS_DIR=	${SRCTOP}/contrib/cheri-c-tests
.if exists(${CHERI_C_TESTS_DIR}/Makefile)
.PATH: ${CHERI_C_TESTS_DIR}
CFLAGS+=	-DCHERI_C_TESTS \
		-I${CHERI_C_TESTS_DIR}

.ifndef BOOTSTRAPPING

CFLAGS+=	-DTEST_CUSTOM_FRAMEWORK -I${CHERITEST_DIR}
TEST_SRCS!=	grep ^DECLARE_TEST ${CHERI_C_TESTS_DIR}/cheri_c_testdecls.h | \
		    sed -e 's/.*(\([^,]*\),.*/\1.c/'
SRCS+=	test_runtime.c	\
	${TEST_SRCS}
.endif
.endif
.endif

MAN=

.ifndef BOOTSTRAPPING
.if ${PROG} == cheritest
NEED_CHERI=	hybrid
.elif ${PROG} == cheriabitest
WANT_CHERI=	pure
.endif
WANT_DUMP=	yes
LIBADD= 	cheri z
.endif

LIBADD+=	xo util

NO_SHARED?=	YES

NO_WERROR=	YES

.ifdef BOOTSTRAPPING
CFLAGS+=	-DLIST_ONLY \
		-I${SRCTOP}/libexec/cheritest-helper \
		-I${SRCTOP}/contrib/libxo
LDFLAGS+=	-L${.OBJDIR}/../../lib/libxo
.endif

.include <bsd.prog.mk>
