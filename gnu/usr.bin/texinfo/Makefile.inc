# $FreeBSD$

TXIDIR=		${.CURDIR}/../../../../contrib/texinfo

.if exists(${.OBJDIR}/../libtxi)
LIBTXI=		${.OBJDIR}/../libtxi/libtxi.a
.else
LIBTXI=		${.CURDIR}/../libtxi/libtxi.a
.endif

CFLAGS+=	-DHAVE_CONFIG_H -DLOCALEDIR=\"/usr/share/locale\" \
		-I${TXIDIR} -I${TXIDIR}/lib

.include "../Makefile.inc"
