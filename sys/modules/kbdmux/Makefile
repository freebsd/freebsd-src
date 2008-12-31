# $Id: Makefile,v 1.1 2005/06/23 00:30:50 max Exp $
# $FreeBSD: src/sys/modules/kbdmux/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/kbdmux

KMOD=	kbdmux
SRCS=	kbdmux.c opt_compat.h opt_kbd.h bus_if.h device_if.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
