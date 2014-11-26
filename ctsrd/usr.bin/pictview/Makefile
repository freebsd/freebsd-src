# $FreeBSD$

SUBDIR=	images

PROG=	pictview

MAN=

WARNS=	0

LDADD+=		-lterasic_mtl -limagebox -lvuln_png
LIBADD+=	cheri util z pthread

.include <bsd.prog.mk>
