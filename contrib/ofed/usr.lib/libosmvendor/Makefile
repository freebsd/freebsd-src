# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

.PATH: ${VENDORLIBDIR}

LIB=		osmvendor
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	osm_vendor_ibumad.c osm_vendor_ibumad_sa.c

CFLAGS+= -DOSM_VENDOR_INTF_OPENIB

VERSION_MAP= ${VENDORLIBDIR}/libosmvendor.map

.include <bsd.lib.mk>
