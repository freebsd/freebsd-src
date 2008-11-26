# $FreeBSD: src/usr.sbin/wpa/hostapd_cli/Makefile,v 1.2.2.1.8.1 2008/10/02 02:57:24 kensmith Exp $

CONTRIB=	${.CURDIR}/../../../contrib
HOSTAPD_DISTDIR?= ${CONTRIB}/hostapd
WPA_SUPPLICANT_DISTDIR?= ${CONTRIB}/wpa_supplicant

.PATH: ${HOSTAPD_DISTDIR} ${WPA_SUPPLICANT_DISTDIR}

PROG=	hostapd_cli
SRCS=	hostapd_cli.c wpa_ctrl.c

MAN=	hostapd_cli.8

.include <bsd.prog.mk>
