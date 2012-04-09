#
# $FreeBSD: src/usr.sbin/usbconfig/Makefile,v 1.2.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $
#
PROG=	usbconfig
MAN=	usbconfig.8
SRCS=	usbconfig.c dump.c
DPADD+=	${LIBUSB}
LDADD+= -lusb

.include <bsd.prog.mk>
