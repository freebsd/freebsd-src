# $FreeBSD$

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

MK_INSTALLLIB=	no
MK_PROFILE=	no

CFLAGS+= -I${PAMDIR}/include -I${.CURDIR}/../../libpam

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
.if defined(_NO_LIBPAM_SO_YET)
NO_PIC=
.else
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
LIBADD+=	pam
.endif

.include "../Makefile.inc"
