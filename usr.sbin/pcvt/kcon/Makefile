# $FreeBSD$
PROG=		kcon
DEVICE=		/dev/ttyv0
CFLAGS+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

.if exists(${.OBJDIR}/../keycap)
LIBKEYCAP=	${.OBJDIR}/../keycap/libkeycap.a
.else
LIBKEYCAP=	${.CURDIR}/../keycap/libkeycap.a
.endif

DPADD=		${LIBKEYCAP}
LDADD=		${LIBKEYCAP}

.include <bsd.prog.mk>
