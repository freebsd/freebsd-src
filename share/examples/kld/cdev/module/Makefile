# Makefile for kld char device driver.
# $FreeBSD: src/share/examples/kld/cdev/module/Makefile,v 1.5 2001/09/18 12:28:22 ru Exp $

KMOD=	cdev
SRCS=	cdev.c cdevmod.c

/dev/cdev:
	mknod ${.TARGET} c 32 0

un/dev/cdev: .PHONY
	rm -f /dev/cdev

.include <bsd.kmod.mk>

load: /dev/cdev
unload: un/dev/cdev
