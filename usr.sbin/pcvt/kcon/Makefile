# $FreeBSD: src/usr.sbin/pcvt/kcon/Makefile,v 1.13 2003/04/04 17:49:16 obrien Exp $

PROG=	kcon
DEVICE=	/dev/ttyv0

CFLAGS+= -I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

DPADD=	${LIBKEYCAP}
LDADD=	-lkeycap

.include <bsd.prog.mk>
