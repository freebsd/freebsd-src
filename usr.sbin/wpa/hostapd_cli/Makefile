# $FreeBSD: src/usr.sbin/wpa/hostapd_cli/Makefile,v 1.6.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${HOSTAPD_DISTDIR}

PROG=	hostapd_cli
SRCS=	hostapd_cli.c wpa_ctrl.c os_unix.c

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX

MAN=	hostapd_cli.8

.include <bsd.prog.mk>
