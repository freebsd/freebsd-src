# $FreeBSD: src/lib/libpam/modules/Makefile.inc,v 1.15.6.1 2005/02/13 07:23:13 obrien Exp $

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

NOINSTALLLIB=
NOPROFILE=

CFLAGS+= -I${PAMDIR}/include -I${.CURDIR}/../../libpam
WARNS?=	4

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
.if defined(_NO_LIBPAM_SO_YET)
NOPIC=
.else
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
DPADD+=		${LIBPAM}
LDADD+=		-lpam
.endif

.include "../Makefile.inc"
