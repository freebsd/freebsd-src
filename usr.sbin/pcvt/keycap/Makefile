# $FreeBSD: src/usr.sbin/pcvt/keycap/Makefile,v 1.16.12.1 2008/10/02 02:57:24 kensmith Exp $

LIB=	keycap
MAN=	keycap.3
MLINKS+= keycap.3 kgetent.3 \
	keycap.3 kgetnum.3 \
	keycap.3 kgetflag.3 \
	keycap.3 kgetstr.3
MAN+=	man5/keycap.5
SRCS=	keycap.c

FILES=	keycap.src
FILESDIR= /usr/share/misc
FILESNAME= keycap.pcvt

CFLAGS+= -DKEYCAP_PATH=\"${FILESDIR}/${FILESNAME}\"

#CLEANFILES+=	keycap.0 man5/keycap.0

.include <bsd.lib.mk>
