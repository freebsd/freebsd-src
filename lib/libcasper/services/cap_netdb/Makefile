# $FreeBSD$

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=	runtime

SHLIB_MAJOR=	1
INCSDIR?=	${INCLUDEDIR}/casper

.if ${MK_CASPER} != "no"
SHLIB=	cap_netdb

SRCS=	cap_netdb.c
.endif

INCS=	cap_netdb.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

MAN+=	cap_netdb.3

MLINKS+=cap_netdb.3 libcap_netdb.3
MLINKS+=cap_netdb.3 cap_getprotobyname.3

.include <bsd.lib.mk>
