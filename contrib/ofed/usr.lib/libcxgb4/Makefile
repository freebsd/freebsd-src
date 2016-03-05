# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

CXGB4DIR=	${.CURDIR}/../../libcxgb4
IBVERBSDIR=	${.CURDIR}/../../libibverbs
CXGBSRCDIR=	${CXGB4DIR}/src

.PATH: ${CXGBSRCDIR}

LIB=		cxgb4
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	dev.c cq.c qp.c verbs.c

LIBADD=	ibverbs pthread
CFLAGS+= -g -DHAVE_CONFIG_H -DDEBUG
CFLAGS+= -I${.CURDIR} -I${CXGBSRCDIR} -I${IBVERBSDIR}/include 

VERSION_MAP= ${CXGBSRCDIR}/cxgb4.map

.include <bsd.lib.mk>
