# $FreeBSD: src/usr.sbin/pcvt/kcon/Makefile,v 1.12 2001/07/30 11:38:35 bde Exp $

PROG=	kcon
DEVICE=	/dev/ttyv0
CFLAGS+= -I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"
DPADD=	${LIBKEYCAP}
LDADD=	-lkeycap

.include <bsd.prog.mk>
