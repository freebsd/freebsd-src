# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

IBCOMMONDIR= 	../../management/libibcommon
IBSRCDIR=	${IBCOMMONDIR}/src
OFEDSYS= 	../../../../sys/ofed

.PATH: ${IBSRCDIR}

LIB=		ibcommon
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	sysfs.c util.c hash.c stack.c time.c

CFLAGS+= -I${.CURDIR} -I${IBSRCDIR} -I${IBCOMMONDIR}/include
CFLAGS+= -I${OFEDSYS}/include -I${IBCOMMONDIR}/include/infiniband

VERSION_MAP= ${IBSRCDIR}/libibcommon.map

.include <bsd.lib.mk>
