# $Id: Makefile,v 1.1 2005/06/23 00:30:50 max Exp $
# $FreeBSD: src/sys/modules/kbdmux/Makefile,v 1.1.2.3.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/kbdmux

KMOD=	kbdmux
SRCS=	kbdmux.c opt_kbd.h bus_if.h device_if.h

.if !defined(KERNBUILDDIR)
opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
