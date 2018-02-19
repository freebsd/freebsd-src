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

LIBADD=		pthread
CFLAGS+=	-I${.CURDIR}/.. -DHAVE_CONFIG_H=1
CFLAGS+=	-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/infiniband
VERSION_MAP=	${.CURDIR}/../opensm/libopensm.map

.include <bsd.lib.mk>
