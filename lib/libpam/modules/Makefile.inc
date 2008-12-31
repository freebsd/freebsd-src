# $FreeBSD: src/lib/libpam/modules/Makefile.inc,v 1.22.6.1 2008/11/25 02:59:29 kensmith Exp $

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

NO_INSTALLLIB=
NO_PROFILE=

CFLAGS+= -I${PAMDIR}/include -I${.CURDIR}/../../libpam
WARNS?=	4

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
.if defined(_NO_LIBPAM_SO_YET)
NO_PIC=
.else
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
DPADD+=		${LIBPAM}
LDADD+=		-lpam
.endif

.include "../Makefile.inc"
