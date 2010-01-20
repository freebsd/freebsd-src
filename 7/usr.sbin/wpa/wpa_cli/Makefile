# $FreeBSD$

WPA_SUPPLICANT_DISTDIR?= ${.CURDIR}/../../../contrib/wpa_supplicant
.PATH: ${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_cli
SRCS=	wpa_cli.c wpa_ctrl.c os_unix.c

MAN=	wpa_cli.8

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX

.include <bsd.prog.mk>
