# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

MTHCADIR=	${.CURDIR}/../../libmthca
IBVERBSDIR=	${.CURDIR}/../../libibverbs
MTHCASRCDIR=	${MTHCADIR}/src

.PATH: ${MTHCASRCDIR}

LIB=		mthca
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	ah.c buf.c cq.c memfree.c mthca.c qp.c srq.c verbs.c


CFLAGS+= -DHAVE_CONFIG_H 
CFLAGS+= -I${.CURDIR} -I${MTHCASRCDIR} -I${IBVERBSDIR}/include

VERSION_MAP= ${MTHCASRCDIR}/mthca.map

.include <bsd.lib.mk>
