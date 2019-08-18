# $FreeBSD$

PACKAGE=	bluetooth
CONFS=		iwmbtfw.conf
CONFSDIR=       /etc/devd
PROG=		iwmbtfw
MAN=		iwmbtfw.8
LIBADD+=	usb
SRCS=		main.c iwmbt_fw.c iwmbt_hw.c

.include <bsd.prog.mk>
