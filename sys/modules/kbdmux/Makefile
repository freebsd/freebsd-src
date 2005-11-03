# $Id: Makefile,v 1.1 2005/06/23 00:30:50 max Exp $
# $FreeBSD: src/sys/modules/kbdmux/Makefile,v 1.1.2.1 2005/07/21 16:41:18 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/kbdmux

KMOD=	kbdmux
SRCS=	kbdmux.c opt_kbd.h

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}

.include <bsd.kmod.mk>
