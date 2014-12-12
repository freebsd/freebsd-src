# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c

.ifndef BOOTSTRAPPING
SRCS+=	cheritest_ccall.c						\
	cheritest_fault.c						\
	cheritest_fd.c							\
	cheritest_inflate.c						\
	cheritest_libcheri.c						\
	cheritest_registers.c						\
	cheritest_sandbox.S						\
	cheritest_stack.c						\
	cheritest_syscall.c						\
	cheritest_util.c						\
	cheritest_var.c							\
	cheritest_vm.c
.endif

MAN=

.ifndef BOOTSTRAPPING
USE_CHERI=	yes
WANT_DUMP=	yes
LIBADD= 	cheri z
.endif

LIBADD+=	xo

NO_SHARED?=	YES

NO_WERROR=	YES

.ifdef BOOTSTRAPPING
CFLAGS+=	-DLIST_ONLY \
		-I${.CURDIR}/../../libexec/cheritest-helper \
		-I${.CURDIR}/../../contrib/libxo
LDFLAGS+=	-L${.OBJDIR}/../../lib/libxo
.endif

.include <src.opts.mk>
.include <bsd.prog.mk>
