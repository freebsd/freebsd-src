# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c							\
	cheritest_ccall.c						\
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

MAN=
USE_CHERI=	yes
WANT_DUMP=	yes

LIBADD=  cheri xo z

NO_SHARED?=	YES

NO_WERROR=	YES

.include <src.opts.mk>
.include <bsd.prog.mk>
