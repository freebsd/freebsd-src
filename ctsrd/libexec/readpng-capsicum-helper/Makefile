# $FreeBSD$

.include <bsd.init.mk>

PROG=	readpng-capsicum-helper

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS=	readpng.c decode_png.c

MAN=

CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
LIBADD+=	vuln_png z

.include <bsd.prog.mk>
