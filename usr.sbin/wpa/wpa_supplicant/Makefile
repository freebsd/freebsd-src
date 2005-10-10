# $FreeBSD$

WPA_SUPPLICANT_DISTDIR?= ${.CURDIR}/../../../contrib/wpa_supplicant
.PATH: ${.CURDIR}/.. ${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_supplicant
SRCS=	config.c eloop.c common.c md5.c rc4.c sha1.c aes_wrap.c \
	wpa_supplicant.c wpa.c \
	ctrl_iface.c l2_packet.c drivers.c driver_freebsd.c \
	driver_ndis.c driver_ndis_.c Packet32.c

MAN=	wpa_supplicant.8 wpa_supplicant.conf.5

CFLAGS+= -I${.CURDIR} -I${WPA_SUPPLICANT_DISTDIR}
CFLAGS+= -DCONFIG_DRIVER_BSD
CFLAGS+= -DCONFIG_DRIVER_NDIS
CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -g
DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

.if defined(ENABLE_WPA_SUPPLICANT_EAPOL)
SRCS+=	eapol_sm.c eap.c
CFLAGS+= -DIEEE8021X_EAPOL

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
CFLAGS+=-DEAP_TLS -DEAP_PEAP -DEAP_MSCHAPv2 -DEAP_LEAP -DEAP_PSK \
	-DEAP_TLV -DEAP_TLS_FUNCS
SRCS+=	eap_tls.c eap_peap.c eap_mschapv2.c eap_leap.c eap_psk.c \
	eap_tlv.c eap_tls_common.c tls_openssl.c ms_funcs.c crypto.c

CFLAGS+=-DEAP_TTLS -DEAP_MD5
SRCS+=	eap_ttls.c eap_md5.c

# NB: requires patch to openssl
#CFLAGS+= -DEAP_FAST
#SRCS+=	eap_fast.c

DPADD+= ${LIBCRYPTO}
LDADD+= -lssl -lcrypto
.else
SRCS+=	tls_none.c
.endif

.endif

.include <bsd.prog.mk>
