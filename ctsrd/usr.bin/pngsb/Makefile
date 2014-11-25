# $FreeBSD$

PROG=	pngsb

MAN=

NO_SHARED=yes
WANT_DUMP=yes

WARNS=	6

CFLAGS+=	-I${.CURDIR}/../../ctsrd-lib/libimagebox/ \
LDADD+=		-lpthread -lterasic_mtl -limagebox -lvuln_png
LIBADD+=	cheri z

.include <bsd.prog.mk>
