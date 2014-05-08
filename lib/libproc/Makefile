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
# avoid cyclic dependency
CFLAGS+=	-I${.CURDIR:H}/librtld_db

.if ${MK_LIBCPLUSPLUS} != "no"
LDADD+=		-lcxxrt
DPADD+=		${LIBCXXRT}
.else
LDADD+=		-lsupc++
DPADD+=		${LIBSTDCPLUSPLUS}
.endif

SHLIB_MAJOR=	2

MAN=

.include <bsd.lib.mk>
