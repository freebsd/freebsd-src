# Makefile for kld char device driver.
# $FreeBSD$

KMOD=	cdev
SRCS=	cdev.c cdevmod.c

/dev/cdev:
	mknod ${.TARGET} c 32 0

un/dev/cdev: .PHONY
	rm -f /dev/cdev

.include <bsd.kmod.mk>

load: /dev/cdev
unload: un/dev/cdev
