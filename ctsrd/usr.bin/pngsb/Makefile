# $FreeBSD$

PROG=	pngsb

MAN=

NO_SHARED=no
WANT_DUMP=yes

WARNS=	6

CFLAGS+=	-I${.CURDIR}/../../ctsrd-lib/libimagebox/ \
		-I${.CURDIR}/../../ctsrd-lib/libvuln_png/
LDADD+=		-lpthread -lterasic_mtl -limagebox -lvuln_png
LIBADD+=	cheri z

.include <bsd.prog.mk>
