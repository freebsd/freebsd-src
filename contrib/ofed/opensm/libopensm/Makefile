# $FreeBSD$

.PATH:	${.CURDIR}/../opensm

SHLIBDIR?=	/usr/lib
LIB=		opensm
SHLIB_MAJOR=	5
MK_PROFILE=	no

SRCS= \
osm_log.c \
osm_mad_pool.c \
osm_helper.c

.if defined(TESTBUILD)
LDFLAGS=	-lpthread
.else
LIBADD=		pthread
.endif
CFLAGS+=	-I${.CURDIR}/.. -DHAVE_CONFIG_H=1
CFLAGS+=	-I${INCLUDEDIR}/infiniband
VERSION_MAP=	${.CURDIR}/../opensm/libopensm.map

.include <bsd.lib.mk>
