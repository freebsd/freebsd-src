# $FreeBSD$

PROG=	kcon
DEVICE=	/dev/ttyv0

CFLAGS+= -I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

DPADD=	${LIBKEYCAP}
LDADD=	-lkeycap

.if exists(${.OBJDIR}/../keycap)
LIBKEYCAP=	${.OBJDIR}/../keycap/libkeycap.a
.else
LIBKEYCAP=	${.CURDIR}/../keycap/libkeycap.a
.endif

.include <bsd.prog.mk>
