# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

UMADDIR=	../../management/libibumad
IBSRCDIR=	${UMADDIR}/src
OFEDSYS= 	../../../../sys/ofed

.PATH: ${IBSRCDIR}

LIB=		ibumad
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	umad.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I/usr/include/infiniband -I${OFEDSYS}/include 

VERSION_MAP= ${IBSRCDIR}/libibumad.map

.include <bsd.lib.mk>
