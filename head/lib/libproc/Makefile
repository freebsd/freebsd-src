# $FreeBSD$

.include <src.opts.mk>

LIB=	proc

SRCS=	proc_bkpt.c		\
	proc_create.c		\
	proc_regs.c		\
	proc_sym.c		\
	proc_rtld.c		\
	proc_util.c

INCS=	libproc.h

CFLAGS+=	-I${.CURDIR}

.if ${MK_CXX} == "no"
CFLAGS+=	-DNO_CXA_DEMANGLE
.elif ${MK_LIBCPLUSPLUS} != "no"
LDADD+=		-lcxxrt
DPADD+=		${LIBCXXRT}
.else
LDADD+=		-lsupc++
DPADD+=		${LIBSTDCPLUSPLUS}
.endif

.if ${MK_CDDL} != "no"
LDADD+=		-lctf
DPADD+=		${LIBCTF}
IGNORE_PRAGMA=	YES
CFLAGS+=	-I${.CURDIR}/../../cddl/contrib/opensolaris/lib/libctf/common \
		-I${.CURDIR}/../../sys/cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../sys/cddl/compat/opensolaris
.else
CFLAGS+=	-DNO_CTF
.endif

SHLIB_MAJOR=	3

MAN=

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
