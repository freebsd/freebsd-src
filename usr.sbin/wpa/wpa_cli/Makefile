# $FreeBSD: src/usr.sbin/wpa/wpa_cli/Makefile,v 1.2.12.1 2008/10/02 02:57:24 kensmith Exp $

WPA_SUPPLICANT_DISTDIR?= ${.CURDIR}/../../../contrib/wpa_supplicant
.PATH: ${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_cli
SRCS=	wpa_cli.c wpa_ctrl.c

MAN=	wpa_cli.8

.include <bsd.prog.mk>
