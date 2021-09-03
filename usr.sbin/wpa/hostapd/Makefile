# $FreeBSD$

.include <src.opts.mk>
.include "../Makefile.inc"

.include "../Makefile.crypto"

.PATH.c:${HOSTAPD_DISTDIR} \
	${WPA_DISTDIR}/src/drivers

PACKAGE=	hostapd
PROG=	hostapd
SRCS=	config_file.c \
	ctrl_iface.c \
	driver_bsd.c \
	eap_register.c \
	main.c

MAN=	hostapd.8 hostapd.conf.5

.if ${MK_EXAMPLES} != "no"
FILESDIR= ${SHAREDIR}/examples/hostapd
.PATH:	${HOSTAPD_DISTDIR}
FILES=	hostapd.conf hostapd.eap_user hostapd.wpa_psk
.endif

CFLAGS+=-I${.CURDIR:H}/wpa_supplicant \
	-I${WPA_DISTDIR}/src/eap_peer \
	-DHOSTAPD
.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif
#CFLAGS+= -g
LIBADD+=	pcap util \
		wpadrivers wpaap wpal2_packet wpaeap_server \
		wpaeapol_auth wpaeap_common \
		wparadius wpatls wpawps wpacommon wpacrypto wpautils

# User customizations for wpa_supplicant/hostapd build environment
CFLAGS+=${HOSTAPD_CFLAGS}
#DPADD+=${HOSTAPD_DPADD}
LDADD+=${HOSTAPD_LDADD}
#LDFLAGS+=${HOSTAPD_LDFLAGS}

# For WPS, EAP modes, etc
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y

.if !empty(CFLAGS:M*-DCONFIG_WPS)
NEED_SIM_COMMON=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_AKA)
NEED_SIM_COMMON=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_SIM)
NEED_SIM_COMMON=y
.endif

.if defined(NEED_SIM_COMMON)
NEED_FIPS186_2_PRF=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
NEED_AES_OMAC1=y
.endif

.include <bsd.prog.mk>
