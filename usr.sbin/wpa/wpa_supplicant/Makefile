# $FreeBSD$

.include <src.opts.mk>

.include "../Makefile.inc"

.include "../Makefile.crypto"

.PATH.c:${WPA_SUPPLICANT_DISTDIR} \
	${WPA_DISTDIR}/src/drivers

PROG=	wpa_supplicant

SRCS=	bss.c \
	bssid_ignore.c \
	config.c \
	config_file.c \
	ctrl_iface.c \
	ctrl_iface_unix.c \
	driver_bsd.c \
	eap_register.c \
	events.c \
	gas_query.c \
	main.c \
	notify.c \
	op_classes.c \
	offchannel.c \
	pasn_supplicant.c \
	robust_av.c \
	rrm.c \
	scan.c \
	twt.c \
	wmm_ac.c \
	wpa_supplicant.c \
	wpas_glue.c

MAN=	wpa_supplicant.8 wpa_supplicant.conf.5

.if ${MK_EXAMPLES} != "no"
FILESDIR= ${SHAREDIR}/examples/etc
.PATH:	${WPA_SUPPLICANT_DISTDIR}
FILES=	wpa_supplicant.conf
.endif

CFLAGS+=-DCONFIG_BACKEND_FILE
#CFLAGS+= -g
LIBADD+=pcap util wpadrivers wpaeapol_supp \
	wpaeap_server \
	wpaeap_peer wpaeap_common wpal2_packet wparsn_supp wpatls wpawps \
	wpacommon wpacrypto wpautils

# User customizations to the wpa_supplicant build environment
CFLAGS+=${WPA_SUPPLICANT_CFLAGS}
#DPADD+=${WPA_SUPPLICANT_DPADD}
LDADD+=${WPA_SUPPLICANT_LDADD}
#LDFLAGS+=${WPA_SUPPLICANT_LDFLAGS}

.if ${MK_WPA_SUPPLICANT_EAPOL} != "no"
CFLAGS+=-DEAP_GTC \
	-DEAP_LEAP \
	-DEAP_MD5 \
	-DEAP_MSCHAPv2 \
	-DEAP_OTP \
	-DEAP_PEAP \
	-DEAP_PSK \
	-DEAP_TLS \
	-DEAP_TTLS \
	-DEAP_WSC \
	-DIEEE8021X_EAPOL
SRCS+=	hs20_supplicant.c \
	interworking.c \
	wps_supplicant.c
NEED_AES_EAX=y
NEED_AES_ENCBLOCK=y
NEED_AES_OMAC1=y
.endif

.if !empty(CFLAGS:M*-DEAP_AKA)
SRCS+=	eap_aka.c
NEED_SIM_COMMON=y
NEED_AES_CBC=y
.endif

.if !empty(CFLAGS:M*-DEAP_SIM)
SRCS+=	eap_sim.c
NEED_SIM_COMMON=y
NEED_AES_CBC=y
.endif

.if defined(NEED_SIM_COMMON)
SRCS+=	eap_sim_common.c
NEED_FIPS186_2_PRF=y
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
LIBADD+=	pcslite pthread
.endif

.if !empty(CFLAGS:M*-DEAP_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
SRCS+=	eap_gpsk.c \
	eap_gpsk_common.c
NEED_AES_OMAC1=y
.endif

.if !empty(CFLAGS:M*-DEAP_PAX)
SRCS+=	eap_pax.c \
	eap_pax_common.c
.endif

.if !empty(CFLAGS:M*-DEAP_SAKE)
SRCS+=	eap_sake.c \
	eap_sake_common.c
.endif

.include <bsd.prog.mk>
