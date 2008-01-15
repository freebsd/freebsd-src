# $FreeBSD: src/lib/libpam/modules/Makefile.inc,v 1.18 2004/12/21 09:51:09 ru Exp $

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
