#	$NetBSD: Makefile,v 1.4 1999/05/11 21:02:25 augustss Exp $
#	$FreeBSD$

.include <bsd.own.mk>

PROG=	usbhidctl
SRCS=	usbhid.c

LDADD+=	-lusbhid
DPADD+=	${LIBUSBHID}

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
