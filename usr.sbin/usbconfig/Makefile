#
# $FreeBSD: src/usr.sbin/usbconfig/Makefile,v 1.2.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $
#
PROG=	usbconfig
MAN=	usbconfig.8
SRCS=	usbconfig.c dump.c
DPADD+=	${LIBUSB}
LDADD+= -lusb

.include <bsd.prog.mk>
