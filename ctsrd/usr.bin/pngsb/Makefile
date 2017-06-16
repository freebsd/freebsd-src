# $FreeBSD$

PROG=	pngsb

MAN=

NO_SHARED=yes
WANT_DUMP=yes

WARNS=	6

LIBADD+=	cheri terasic_mtl imagebox
# we need the png.h header but don't need to link against it
CFLAGS+=        -I${SRCTOP}/contrib/libpng
CFLAGS+=        -I${SRCTOP}/lib/libpng


.include <bsd.prog.mk>
