# $FreeBSD$

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

NOINSTALLLIB=	yes
NOPROFILE=	yes

CFLAGS+=	-I${PAMDIR}/include
CFLAGS+=	-I${.CURDIR}/../../libpam
WARNS?=		4
NO_WERROR=	yes

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
.if defined(_NO_LIBPAM_SO_YET)
NOPIC=		YES
.else
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
DPADD+=		${LIBPAM}
LDADD+=		-lpam
.endif

.include	"../Makefile.inc"
