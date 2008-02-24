# $FreeBSD: src/usr.sbin/wpa/hostapd_cli/Makefile,v 1.4 2007/07/09 16:26:48 sam Exp $

CONTRIB=	${.CURDIR}/../../../contrib
HOSTAPD_DISTDIR?= ${CONTRIB}/hostapd
WPA_SUPPLICANT_DISTDIR?= ${CONTRIB}/wpa_supplicant

.PATH: ${HOSTAPD_DISTDIR} ${WPA_SUPPLICANT_DISTDIR}

PROG=	hostapd_cli
SRCS=	hostapd_cli.c wpa_ctrl.c

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX

MAN=	hostapd_cli.8

.include <bsd.prog.mk>
