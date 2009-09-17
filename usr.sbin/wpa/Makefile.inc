# $FreeBSD$

BINDIR?=	/usr/sbin

WPA_DISTDIR?= 		${.CURDIR}/../../../contrib/wpa/
WPA_SUPPLICANT_DISTDIR?=${WPA_DISTDIR}/wpa_supplicant
HOSTAPD_DISTDIR?=	${WPA_DISTDIR}/hostapd

.PATH.c:${.CURDIR}/.. \
	${WPA_DISTDIR}/src/common \
	${WPA_DISTDIR}/src/crypto \
	${WPA_DISTDIR}/src/eap_common \
	${WPA_DISTDIR}/src/eapol_supp \
	${WPA_DISTDIR}/src/l2_packet \
	${WPA_DISTDIR}/src/utils

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${WPA_DISTDIR}/src
CFLAGS+=-I${WPA_DISTDIR}/src/common
CFLAGS+=-I${WPA_DISTDIR}/src/crypto
CFLAGS+=-I${WPA_DISTDIR}/src/l2_packet
CFLAGS+=-I${WPA_DISTDIR}/src/utils

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX

.include <bsd.own.mk>
