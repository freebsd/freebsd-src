# $FreeBSD$

OBJTOP=		${.OBJDIR:H:H:H:H:H}
SRCTOP=		${.CURDIR:H:H:H:H:H}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libc/gen/${.CURDIR:T}

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/gen/execve

NETBSD_ATF_TESTS_C=	execve_test

.include "../../Makefile.netbsd-tests"

.include <bsd.test.mk>
