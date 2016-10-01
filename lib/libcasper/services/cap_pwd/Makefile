# $FreeBSD$

.include <src.opts.mk>

PACKAGE=libcasper
LIB=	cap_pwd

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_pwd.c

INCS=	cap_pwd.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
