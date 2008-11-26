# $FreeBSD: src/usr.sbin/pcvt/kcon/Makefile,v 1.13.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	kcon
DEVICE=	/dev/ttyv0

CFLAGS+= -I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

DPADD=	${LIBKEYCAP}
LDADD=	-lkeycap

.include <bsd.prog.mk>
