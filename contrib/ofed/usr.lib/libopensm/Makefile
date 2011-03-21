# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

.PATH: ${OPENSMDIR}/opensm

LIB=		opensm
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	osm_log.c osm_mad_pool.c osm_helper.c

VERSION_MAP= ${OPENSMDIR}/opensm/libopensm.map

.include <bsd.lib.mk>
