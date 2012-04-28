# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${WPA_DISTDIR}/src/drivers

PROG=	wpa_supplicant
SRCS=	aes-cbc.c \
	aes-ctr.c \
	aes-eax.c \
	aes-encblock.c \
	aes-internal.c \
	aes-omac1.c \
	aes-unwrap.c \
	aes-wrap.c \
	base64.c \
	blacklist.c \
	bss.c \
	common.c \
	config.c \
	config_file.c \
	ctrl_iface.c \
	ctrl_iface_unix.c \
	driver_ndis.c \
	driver_wired.c \
	drivers.c \
	eap_register.c \
	eloop.c \
	events.c \
	main.c \
	md5.c \
	notify.c \
	os_unix.c \
	peerkey.c \
	pmksa_cache.c \
	preauth.c \
	scan.c \
	sha1-pbkdf2.c \
	sha1-tlsprf.c \
	sha1-tprf.c \
	sha1.c \
	wpa.c \
	wpa_common.c \
	wpa_debug.c \
	wpa_ie.c \
	wpa_supplicant.c \
	wpabuf.c \
	wpas_glue.c
SRCS+=	driver_freebsd.c \
	l2_packet_freebsd.c \
	Packet32.c

MAN=	wpa_supplicant.8 wpa_supplicant.conf.5

.if ${MK_EXAMPLES} != "no"
FILESDIR= ${SHAREDIR}/examples/etc
.PATH:	${WPA_SUPPLICANT_DISTDIR}
FILES=	wpa_supplicant.conf
.endif

CFLAGS+=-DCONFIG_BACKEND_FILE \
	-DCONFIG_DEBUG_SYSLOG \
	-DCONFIG_DRIVER_BSD \
	-DCONFIG_DRIVER_NDIS \
	-DCONFIG_DRIVER_WIRED \
	-DCONFIG_PEERKEY \
	-DCONFIG_SMARTCARD \
	-DCONFIG_TERMINATE_ONLASTIF \
	-DPKCS12_FUNCS
#CFLAGS+= -g
DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

# User customizations to the wpa_supplicant build environment
CFLAGS+=${WPA_SUPPLICANT_CFLAGS}
#DPADD+=${WPA_SUPPLICANT_DPADD}
LDADD+=${WPA_SUPPLICANT_LDADD}
#LDFLAGS+=${WPA_SUPPLICANT_LDFLAGS}

.if ${MK_WPA_SUPPLICANT_EAPOL} != "no"

.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)

CFLAGS+=-DEAP_GTC \
	-DEAP_LEAP \
	-DEAP_MD5 \
	-DEAP_MSCHAPv2 \
	-DEAP_OTP \
	-DEAP_PEAP \
	-DEAP_PSK \
	-DEAP_TLS \
	-DEAP_TLV \
	-DEAP_TLS_FUNCS \
	-DEAP_TLS_OPENSSL \
	-DEAP_TTLS \
	-DIEEE8021X_EAPOL
SRCS+=	chap.c \
	crypto_openssl.c \
	eap.c \
	eap_common.c \
	eap_gtc.c \
	eap_leap.c \
	eap_md5.c \
	eap_methods.c \
	eap_mschapv2.c \
	eap_otp.c \
	eap_peap.c \
	eap_peap_common.c \
	eap_psk.c \
	eap_psk_common.c \
	eap_tls.c \
	eap_tls_common.c \
	eap_ttls.c \
	eapol_supp_sm.c \
	ms_funcs.c \
	mschapv2.c \
	tls_openssl.c

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
.endif

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

.if !empty(CFLAGS:M*-DEAP_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
SRCS+=	eap_gpsk.c \
	eap_gpsk_common.c
NEED_SHA256=	true
.endif

.if !empty(CFLAGS:M*-DEAP_PAX)
SRCS+=	eap_pax.c \
	eap_pax_common.c
.endif

.if !empty(CFLAGS:M*-DEAP_SAKE)
SRCS+=	eap_sake.c \
	eap_sake_common.c
.endif

NEED_LIBSSL=	true
.else
CFLAGS+= -DEAP_TLS_NONE
SRCS+=	tls_none.c
.endif

.endif

#
# Configure crypto/cipher support.
#
# EAPOL support requires openssl in which case we use their
# cipher code.  Otherwise we use our internal versions.
#
.if !defined(NEED_LIBSSL)
CFLAGS+= -DINTERNAL_AES
CFLAGS+= -DINTERNAL_SHA1
CFLAGS+= -DINTERNAL_MD5
.else
DPADD+= ${LIBSSL} ${LIBCRYPTO}
LDADD+= -lssl -lcrypto
.endif

.if defined(NEED_SHA256)
CFLAGS+=-DINTERNAL_SHA256
SRCS+=	sha256.c
.endif

.include <bsd.prog.mk>
