# $FreeBSD$

SUBDIR=	images

PROG=	cheripoint

MAN=

LDADD+=		-lterasic_mtl -limagebox -lvuln_png
LIBADD+=	cheri util z pthread

NO_SHARED=yes
WANT_DUMP=yes

.include <bsd.prog.mk>
