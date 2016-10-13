# $FreeBSD$

PKG=		clibs
SHLIB_NAME=	libgcc_s.so.1

WARNS?=	2

LDFLAGS+=	-nodefaultlibs
VERSION_MAP=	${.CURDIR}/Version.map

.include "../libcompiler_rt/Makefile.inc"
.include "../libgcc_eh/Makefile.inc"

.include <bsd.lib.mk>
