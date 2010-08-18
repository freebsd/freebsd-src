# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

IBMADDIR= 	../../management/libibmad
UMADDIR=	../../management/libibumad/include
IBSRCDIR=	${IBMADDIR}/src
OFEDSYS= 	../../../../sys/ofed

.PATH: ${IBSRCDIR}

LIB=		ibmad
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	dump.c fields.c gs.c mad.c portid.c register.c resolve.c rpc.c sa.c \
	serv.c smp.c vendor.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I${IBSRCDIR} -I${IBMADDIR}/include
CFLAGS+= -I/usr/include/infiniband -I${OFEDSYS}/include

VERSION_MAP= ${IBSRCDIR}/libibmad.map

.include <bsd.lib.mk>
