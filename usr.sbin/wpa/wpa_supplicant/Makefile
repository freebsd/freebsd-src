# $FreeBSD$

.include <bsd.own.mk>

WPA_SUPPLICANT_DISTDIR?= ${.CURDIR}/../../../contrib/wpa_supplicant
.PATH: ${.CURDIR}/.. ${WPA_SUPPLICANT_DISTDIR}

PROG=	wpa_supplicant
SRCS=	config.c eloop.c common.c md5.c rc4.c sha1.c aes_wrap.c \
	wpa_supplicant.c events.c wpa.c preauth.c pmksa_cache.c \
	ctrl_iface.c ctrl_iface_unix.c l2_packet.c main.c drivers.c \
	driver_ndis.c Packet32.c \
	driver_freebsd.c os_unix.c

MAN=	wpa_supplicant.8 wpa_supplicant.conf.5

.if ${MK_EXAMPLES} != "no"
FILESDIR= ${SHAREDIR}/examples/wpa_supplicant
FILES=	wpa_supplicant.conf
.endif

CFLAGS+= -I${.CURDIR} -I${WPA_SUPPLICANT_DISTDIR}
CFLAGS+= -DCONFIG_DRIVER_BSD
CFLAGS+= -DCONFIG_DRIVER_NDIS
CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX
CFLAGS+= -DCONFIG_TERMINATE_ONLASTIF
CFLAGS+= -g
DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

# NB: we only support wpa_supplicant.conf file
SRCS+=	config_file.c base64.c
CFLAGS+=-DCONFIG_BACKEND_FILE

# User customizations to the wpa_supplicant build environment
CFLAGS+=${WPA_SUPPLICANT_CFLAGS}
#DPADD+=${WPA_SUPPLICANT_DPADD}
LDADD+=${WPA_SUPPLICANT_LDADD}
#LDFLAGS+=${WPA_SUPPLICANT_LDFLAGS}

.if ${MK_WPA_SUPPLICANT_EAPOL} != "no"
SRCS+=	eapol_sm.c eap.c eap_methods.c
CFLAGS+= -DIEEE8021X_EAPOL

.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)
CFLAGS+=-DEAP_TLS -DEAP_PEAP -DEAP_MSCHAPv2 -DEAP_LEAP -DEAP_PSK \
	-DEAP_TLV -DEAP_TLS_FUNCS -DEAP_TLS_OPENSSL
SRCS+=	eap_tls.c eap_peap.c eap_mschapv2.c eap_leap.c \
	eap_psk.c eap_psk_common.c \
	eap_tlv.c eap_tls_common.c tls_openssl.c ms_funcs.c crypto.c

CFLAGS+=-DEAP_TTLS -DEAP_MD5
SRCS+=	eap_ttls.c eap_md5.c

.if !empty(CFLAGS:M*-DEAP_GTC)
SRCS+=	eap_gtc.c
.endif

.if !empty(CFLAGS:M*-DEAP_OTP)
SRCS+=	eap_otp.c
.endif

.if !empty(CFLAGS:M*-DEAP_AKA)
NEED_SIM_COMMON=	true
SRCS+=	eap_aka.c
.endif

.if !empty(CFLAGS:M*-DEAP_SIM)
NEED_SIM_COMMON=	true
SRCS+=	eap_sim.c
.endif

.if defined(NEED_SIM_COMMON)
SRCS+=	eap_sim_common.c

# PC/SC interface for smartcards (USIM, GSM SIM)
# GSM/UMTS authentication algorithm (for EAP-SIM/EAP-AKA)
# NB: requires devel/pcsc-lite
#
# WPA_SUPPLICANT_CFLAGS=-DEAP_AKA -DPCSC_FUNCS -I/usr/local/include/PCSC
# WPA_SUPPLICANT_LDADD=-L/usr/local/lib
#
.if !empty(CFLAGS:M*-DPCSC_FUNCS)
SRCS+=	pcsc_funcs.c
DPADD+=${LIBPTHREAD}
LDADD+=-lpcsclite -lpthread
.endif
.endif

.if !empty(CFLAGS:M*-DEAP_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
SRCS+=	eap_gpsk.c eap_gpsk_common.c
NEED_SHA256=	true
.endif

.if !empty(CFLAGS:M*-DEAP_PAX)
SRCS+=	eap_pax.c eap_pax_common.c
.endif

.if !empty(CFLAGS:M*-DEAP_SAKE)
SRCS+=	eap_sake.c eap_sake_common.c
.endif

.if defined(NEED_SHA256)
CFLAGS+=-DINTERNAL_SHA256
SRCS+=	sha256.c
.endif

# NB: requires patch to openssl
#CFLAGS+= -DEAP_FAST
#SRCS+=	eap_fast.c

DPADD+= ${LIBSSL} ${LIBCRYPTO}
LDADD+= -lssl -lcrypto
.else
CFLAGS+= -DEAP_TLS_NONE
CFLAGS+= -DINTERNAL_AES
CFLAGS+= -DINTERNAL_SHA1
CFLAGS+= -DINTERNAL_MD5
SRCS+=	tls_none.c
.endif

.endif

.include <bsd.prog.mk>
