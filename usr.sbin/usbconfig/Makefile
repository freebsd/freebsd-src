#
# $FreeBSD: src/usr.sbin/usbconfig/Makefile,v 1.2.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $
#
PROG=	usbconfig
MAN=	usbconfig.8
SRCS=	usbconfig.c dump.c
DPADD+=	${LIBUSB}
LDADD+= -lusb

.include <bsd.prog.mk>
