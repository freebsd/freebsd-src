# $FreeBSD$

PROG=	pngsb

MAN=

WARNS=	6

CFLAGS+=	-I${.CURDIR}/../../ctsrd-lib/libimagebox/ \
		-I${.CURDIR}/../../ctsrd-lib/libvuln_png/
LDADD+=		-lpthread -lterasic_mtl -limagebox -lvuln_png -lz -lm -lcheri

.include <bsd.prog.mk>
