# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

IBMADDIR= 	../../management/libibmad
COMMONDIR=	../../management/libibcommon/include
UMADDIR=	../../management/libibumad
IBSRCDIR=	${UMADDIR}/src
OFEDSYS= 	../../../../sys/ofed

.PATH: ${IBSRCDIR}

LIB=		ibumad
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	umad.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${IBSRCDIR} -I${IBMADDIR}/include -I${COMMONDIR}
CFLAGS+= -I${OFEDSYS}/include -I${UMADDIR}/include/infiniband

VERSION_MAP= ${IBSRCDIR}/libibumad.map

.include <bsd.lib.mk>
