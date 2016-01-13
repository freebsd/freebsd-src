# $FreeBSD$

# Hack to build both CheriABI and non-CheriABI versions of cheritest.
.PATH:	${.CURDIR}/../cheritest

PROG=	cheriabitest
SRCS=	cheritest.c

.ifndef BOOTSTRAPPING
SRCS+=	cheritest_bounds_stack.c					\
	cheritest_ccall.c						\
	cheritest_fault.c						\
	cheritest_fd.c							\
	cheritest_inflate.c						\
	cheritest_libcheri.c						\
	cheritest_registers.c						\
	cheritest_sandbox.S						\
	cheritest_string.c						\
	cheritest_syscall.c						\
	cheritest_trusted_stack.c					\
	cheritest_util.c						\
	cheritest_var.c							\
	cheritest_vm.c							\
	cheritest_vm_swap.c
.endif

MAN=

.ifndef BOOTSTRAPPING
WANT_CHERI=pure
WANT_DUMP=yes
LIBADD= 	cheri z
.endif

LIBADD+=	xo util

NO_SHARED?=	YES

NO_WERROR=	YES

.ifdef BOOTSTRAPPING
CFLAGS+=	-DLIST_ONLY \
		-I${.CURDIR}/../../libexec/cheritest-helper \
		-I${.CURDIR}/../../contrib/libxo
LDFLAGS+=	-L${.OBJDIR}/../../lib/libxo
.endif

.include <bsd.prog.mk>
