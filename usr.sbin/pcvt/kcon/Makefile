# $FreeBSD$

PROG=	kcon
DEVICE=	/dev/ttyv0

CFLAGS+= -I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

DPADD=	${LIBKEYCAP}
LDADD=	-lkeycap

.include <bsd.prog.mk>
