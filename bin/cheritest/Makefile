# $FreeBSD$

PROG?=	cheritest
SRCS=	cheritest.c

.ifndef BOOTSTRAPPING
SRCS+=	cheritest_bounds_globals.c					\
	cheritest_bounds_globals_x.c					\
	cheritest_bounds_heap.c						\
	cheritest_bounds_stack.c					\
	cheritest_ccall.c						\
	cheritest_fault.c						\
	cheritest_fd.c							\
	cheritest_libcheri.c						\
	cheritest_libcheri_cxx.cc					\
	cheritest_libcheri_local.c					\
	cheritest_libcheri_trustedstack.c				\
	cheritest_libcheri_var.c					\
	cheritest_registers.c						\
	cheritest_sandbox.S						\
	cheritest_sealcap.c						\
	cheritest_string.c						\
	cheritest_syscall.c						\
	cheritest_util.c						\
	cheritest_vm.c							\
	cheritest_vm_swap.c						\
	cheritest_zlib.c
.endif

CHERITEST_DIR:=	${.PARSEDIR}

.ifdef CHERIABI_TESTS
.ifndef BOOTSTRAPPING
SRCS+= cheritest_cheriabi.c
SRCS+= cheritest_cheriabi_open.c
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

CFLAGS+=	-DTEST_CUSTOM_FRAMEWORK -I${CHERITEST_DIR} \
		-DHAVE_MALLOC_USUABLE_SIZE
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
		-I${SRCTOP}/contrib/libxo \
		-I${.OBJDIR}
LDFLAGS+=	-L${.OBJDIR}/../../lib/libxo
CLEAN_FILES+=	cheritest_list_only.h
SRCS+=		cheritest_list_only.h
# XXX-BD: .PARSEDIR should work for SRCDIR, but sometimes is ""
SRCDIR?=	${.CURDIR}
cheritest_list_only.h:	cheritest.c gen_cheritest_list_only.awk
	awk -f ${SRCDIR}/gen_cheritest_list_only.awk \
	    ${SRCDIR}/cheritest.c > ${.TARGET} || rm -f ${.TARGET}
.endif

.include <bsd.prog.mk>
