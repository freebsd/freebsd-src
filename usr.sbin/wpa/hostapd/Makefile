# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${WPA_DISTDIR}/src/drivers

PROG=	hostapd
SRCS=	accounting.c \
	aes-wrap.c \
	ap_config.c \
	ap_drv_ops.c \
	ap_mlme.c \
	authsrv.c \
	chap.c \
	common.c \
	config_file.c \
	crypto_openssl.c \
	ctrl_iface.c \
	ctrl_iface_ap.c \
	drivers.c \
	drv_callbacks.c \
	eap_common.c \
	eap_peap_common.c \
	eap_register.c \
	eapol_auth_dump.c \
	eapol_auth_sm.c \
	eap_server.c \
	eap_server_methods.c \
	eloop.c \
	hostapd.c \
	ieee802_11_auth.c \
	ieee802_11_common.c \
	ieee802_1x.c \
	ip_addr.c \
	main.c \
	md5.c \
	ms_funcs.c \
	os_unix.c \
	peerkey_auth.c \
	pmksa_cache_auth.c \
	preauth_auth.c \
	radius.c \
	radius_client.c \
	sha1-pbkdf2.c \
	sha1-tlsprf.c \
	sha1.c \
	sta_info.c \
	tkip_countermeasures.c \
	utils.c \
	vlan_init.c \
	wpa_auth.c \
	wpa_auth_glue.c \
	wpa_auth_ie.c \
	wpa_common.c \
	wpa_debug.c \
	wpabuf.c
SRCS+=	l2_packet_freebsd.c driver_freebsd.c

MAN=	hostapd.8 hostapd.conf.5

.if ${MK_EXAMPLES} != "no"
FILESDIR= ${SHAREDIR}/examples/hostapd
.PATH:	${HOSTAPD_DISTDIR}
FILES=	hostapd.conf hostapd.eap_user hostapd.wpa_psk
.endif

CFLAGS+=-DCONFIG_DRIVER_BSD \
	-DHOSTAPD \
	-DCONFIG_DRIVER_RADIUS_ACL \
	-DCONFIG_RSN_PREAUTH \
	-DCONFIG_PEERKEY
.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif
#CFLAGS+= -g
DPADD+=	${LIBPCAP} ${LIBSSL}
LDADD+=	-lpcap -lssl

# User customizations for wpa_supplicant/hostapd build environment
CFLAGS+=${HOSTAPD_CFLAGS}
#DPADD+=${HOSTAPD_DPADD}
LDADD+=${HOSTAPD_LDADD}
#LDFLAGS+=${HOSTAPD_LDFLAGS}

.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)

CFLAGS+=-DDPKCS12_FUNCS \
	-DEAP_SERVER \
	-DEAP_SERVER_GTC \
	-DEAP_SERVER_IDENTITY \
	-DEAP_SERVER_MD5 \
	-DEAP_SERVER_MSCHAPV2 \
	-DEAP_SERVER_PEAP \
	-DEAP_SERVER_TLS \
	-DEAP_SERVER_TTLS \
	-DEAP_TLS_FUNCS \
	-DCONFIG_NO_DUMP_STATE
SRCS+=	dump_state.c \
	eap_server_gtc.c \
	eap_server_identity.c \
	eap_server_md5.c \
	eap_server_mschapv2.c \
	eap_server_peap.c \
	eap_server_tls.c \
	eap_server_tls_common.c \
	eap_server_ttls.c \
	tls_openssl.c

.if !empty(CFLAGS:M*-DEAP_SERVER_AKA)
NEED_SIM_COMMON=	true
NEED_SHA256=	true
SRCS+=	eap_server_aka.c
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_SIM)
NEED_SIM_COMMON=	true
SRCS+=	eap_server_sim.c
.endif

.if defined(NEED_SIM_COMMON)
SRCS+=	eap_sim_common.c \
	eap_sim_db.c
NEED_AES_CBC=	true
NEED_FIPS186_2_PRF=	true
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
SRCS+=	eap_server_gpsk.c \
	eap_gpsk_common.c
NEED_SHA256=	true
NEED_AES_OMAC1=	true
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_PAX)
SRCS+=	eap_server_pax.c \
	eap_pax_common.c
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_SAKE)
SRCS+=	eap_server_sake.c \
	eap_sake_common.c
.endif

DPADD+= ${LIBSSL} ${LIBCRYPTO}
LDADD+= -lssl -lcrypto
.else
NEED_TLS_NONE=	true
.endif

.if defined(NEED_AES_CBC)
SRCS+=	aes-cbc.c
.endif

.if defined(NEED_AES_OMAC1)
SRCS+=	aes-omac1.c
.endif

.if defined(NEED_FIPS186_2_PRF)
SRCS+=	fips_prf_openssl.c
.endif

.if defined(NEED_SHA256)
SRCS+=	sha256.c
.endif

.if defined(NEED_TLS_NONE)
CFLAGS+= -DEAP_TLS_NONE
CFLAGS+= -DINTERNAL_AES
CFLAGS+= -DINTERNAL_SHA1
CFLAGS+= -DINTERNAL_MD5
SRCS+=	tls_none.c
.endif

.include <bsd.prog.mk>
