# $FreeBSD$

PROG=	gpioctl
MAN=	gpioctl.8

CFLAGS+= -I${.CURDIR}/../../lib/libgpio

DPADD+=	${LIBGPIO}
LDADD+= -lgpio

.include <bsd.prog.mk>
