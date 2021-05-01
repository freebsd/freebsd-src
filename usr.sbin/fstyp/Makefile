# $FreeBSD$

.include <src.opts.mk>

PROG=	fstyp

SRCS=	fstyp.c

MAN=	fstyp.8

WARNS?=	2

.if ${MK_ICONV} == "yes"
CFLAGS+=	-DWITH_ICONV
.endif

.include <bsd.endian.mk>

.if ${TARGET_ENDIANNESS} == 1234
HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests
.endif

LIBADD=	fstyp

.include <bsd.prog.mk>
