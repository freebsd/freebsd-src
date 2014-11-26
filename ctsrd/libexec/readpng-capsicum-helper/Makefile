# $FreeBSD$

.include <bsd.init.mk>

PROG=	readpng-capsicum-helper

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS=	readpng.c decode_png.c

MAN=

CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-I/usr/local/include
LDADD+=		-L/usr/local/lib -lpng -lm
.else
#CFLAGS+=	-I${.CURDIR}/../../lib/libvuln_png
LDADD+=		-lvuln_png
.endif
LIBADD+=	z

.include <bsd.prog.mk>
