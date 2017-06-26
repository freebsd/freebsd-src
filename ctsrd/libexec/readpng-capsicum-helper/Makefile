# $FreeBSD$

.include <bsd.init.mk>

PROG=	readpng-capsicum-helper

.PATH: ${SRCTOP}/ctsrd/lib/libimagebox
SRCS=	readpng.c decode_png.c
# we need the png.h header but don't need to link against it
CFLAGS+=        -I${SRCTOP}/contrib/libpng
CFLAGS+=        -I${SRCTOP}/lib/libpng

MAN=

CFLAGS+=	-I${SRCTOP}/ctsrd/lib/libimagebox
LIBADD+=	vuln_png z

.include <bsd.prog.mk>
