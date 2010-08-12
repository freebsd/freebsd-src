# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

OPENSMDIR= 	../../management/opensm/opensm
OFEDSYS= 	../../../../sys/ofed

.PATH: ${OPENSMDIR}

LIB=		opensm
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	osm_log.c osm_mad_pool.c osm_helper.c

CFLAGS+= -I${.CURDIR} -I/usr/include/infiniband -I${OFEDSYS}/include

VERSION_MAP= ${OPENSMDIR}/libopensm.map

.include <bsd.lib.mk>
