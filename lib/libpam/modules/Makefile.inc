# $FreeBSD$

PAMDIR=		${.CURDIR}/../../../../contrib/libpam

INTERNALLIB=	yes
INTERNALSTATICLIB=yes

CFLAGS+=	-I${PAMDIR}/libpam/include
CFLAGS+=	-I${.CURDIR}/../../libpam

# This is nasty.
# For the static case, libpam.a depends on the modules.
# For the dynamic case, the modules depend on libpam.so.N
# Punt for the time being until I can figure out how to do it.
#DPADD+=		${LIBPAM}
#LDADD+=		-lpam

.include	"../Makefile.inc"
