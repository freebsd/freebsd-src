# $FreeBSD$

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper
LIB=	cap_sysctl

SHLIB_MAJOR=	0
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_sysctl.c

INCS=	cap_sysctl.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
