#	$NetBSD: Makefile,v 1.4 1999/05/11 21:02:25 augustss Exp $
#	$FreeBSD: src/usr.bin/usbhidctl/Makefile,v 1.2.2.1 2002/04/03 16:48:25 joe Exp $

.include <bsd.own.mk>

PROG=	usbhidctl
SRCS=	usbhid.c

LDADD+=	-lusbhid
DPADD+=	${LIBUSBHID}

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
