# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

IBMADDIR= 	${.CURDIR}/../../management/libibmad
UMADDIR=	${.CURDIR}/../../management/libibumad/include
IBSRCDIR=	${IBMADDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibmad
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	dump.c fields.c gs.c mad.c portid.c register.c resolve.c rpc.c sa.c \
	serv.c smp.c vendor.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${IBSRCDIR} -I${IBMADDIR}/include
CFLAGS+= -I/usr/include/infiniband

VERSION_MAP= ${IBSRCDIR}/libibmad.map

.include <bsd.lib.mk>
