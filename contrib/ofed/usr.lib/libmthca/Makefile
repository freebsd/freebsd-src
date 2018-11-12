# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

MTHCADIR=	${.CURDIR}/../../libmthca
IBVERBSDIR=	${.CURDIR}/../../libibverbs
MTHCASRCDIR=	${MTHCADIR}/src

.PATH: ${MTHCASRCDIR}

LIB=		mthca
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	ah.c buf.c cq.c memfree.c mthca.c qp.c srq.c verbs.c

LIBADD=	ibverbs pthread
CFLAGS+= -DHAVE_CONFIG_H 
CFLAGS+= -I${.CURDIR} -I${MTHCASRCDIR} -I${IBVERBSDIR}/include

VERSION_MAP= ${MTHCASRCDIR}/mthca.map

.include <bsd.lib.mk>
