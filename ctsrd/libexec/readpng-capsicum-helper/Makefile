# $FreeBSD$

.include <bsd.init.mk>

PROG=	readpng-capsicum-helper

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS=	readpng.c decode_png.c

MAN=

CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-I/usr/local/include
LDADD+=		-L/usr/local/lib -lpng
LIBADD+=	m z
.else
LIBADD+=	vuln_png z
.endif

.include <bsd.prog.mk>
