# $FreeBSD$

.include <bsd.init.mk>

PROG=	readpng-capsicum-helper

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS=	readpng.c decode_png.c
# we need the png.h header but don't need to link against it
CFLAGS+=        -I${.CURDIR}/../../../contrib/libpng
CFLAGS+=        -I${.CURDIR}/../../../lib/libpng

MAN=

CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
LIBADD+=	vuln_png z

.include <bsd.prog.mk>
