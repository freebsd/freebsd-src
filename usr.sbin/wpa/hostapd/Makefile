# $FreeBSD$

.include <src.opts.mk>
.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${HOSTAPD_DISTDIR} \
	${WPA_DISTDIR}/src/drivers

PROG=	hostapd
SRCS=	accounting.c aes-omac1.c ap_config.c ap_drv_ops.c ap_mlme.c authsrv.c \
	base64.c beacon.c bss_load.c chap.c common.c config_file.c \
	ctrl_iface.c \
	ctrl_iface_ap.c driver_common.c l2_packet_freebsd.c driver_bsd.c \
	drivers.c drv_callbacks.c eap_common.c eap_peap_common.c \
	eap_register.c eap_server.c eap_server_methods.c eap_user_db.c \
	eapol_auth_dump.c eapol_auth_sm.c eloop.c gas.c gas_serv.c hostapd.c \
	hs20.c http_client.c http_server.c httpread.c \
	hw_features_common.c ieee802_11_auth.c \
	ieee802_11_common.c ieee802_11_shared.c ieee802_1x.c ip_addr.c \
	main.c ms_funcs.c os_unix.c peerkey_auth.c pmksa_cache_auth.c \
	preauth_auth.c radius.c radius_client.c radius_das.c sta_info.c \
	tkip_countermeasures.c upnp_xml.c utils.c uuid.c vlan_init.c \
	wpa_auth.c wpa_auth_glue.c wpa_auth_ie.c wpa_common.c wpa_debug.c \
	wpabuf.c wps.c wps_attr_build.c wps_attr_parse.c wps_attr_process.c \
	wps_common.c wps_dev_attr.c wps_enrollee.c wps_hostapd.c \
	wps_registrar.c wps_upnp.c wps_upnp_ap.c wps_upnp_event.c \
	wps_upnp_ssdp.c wps_upnp_web.c

MAN=	hostapd.8 hostapd.conf.5

.if ${MK_EXAMPLES} != "no"
FILESDIR= ${SHAREDIR}/examples/hostapd
.PATH:	${HOSTAPD_DISTDIR}
FILES=	hostapd.conf hostapd.eap_user hostapd.wpa_psk
.endif

CFLAGS+=-DCONFIG_DRIVER_BSD \
	-DCONFIG_DRIVER_RADIUS_ACL \
	-DCONFIG_HS20 \
	-DCONFIG_INTERWORKING \
	-DCONFIG_PEERKEY \
	-DCONFIG_RSN_PREAUTH \
	-DCONFIG_WPS \
	-DCONFIG_WPS2 \
	-DCONFIG_WPS_UPNP \
	-DHOSTAPD
.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif
#CFLAGS+= -g
LIBADD+=	pcap util

# User customizations for wpa_supplicant/hostapd build environment
CFLAGS+=${HOSTAPD_CFLAGS}
#DPADD+=${HOSTAPD_DPADD}
LDADD+=${HOSTAPD_LDADD}
#LDFLAGS+=${HOSTAPD_LDFLAGS}

CFLAGS+=-DDPKCS12_FUNCS \
	-DEAP_SERVER \
	-DEAP_SERVER_GTC \
	-DEAP_SERVER_IDENTITY \
	-DEAP_SERVER_MD5 \
	-DEAP_SERVER_MSCHAPV2 \
	-DEAP_SERVER_PEAP \
	-DEAP_SERVER_TLS \
	-DEAP_SERVER_TTLS \
	-DEAP_SERVER_WSC \
	-DEAP_TLS_FUNCS

SRCS+=	eap_server_gtc.c \
	eap_server_identity.c \
	eap_server_md5.c \
	eap_server_mschapv2.c \
	eap_server_peap.c \
	eap_server_tls.c \
	eap_server_tls_common.c \
	eap_server_ttls.c \
	eap_server_wsc.c \
	eap_wsc_common.c
TLS_FUNCS=y

.if !empty(CFLAGS:M*-DCONFIG_WPS)
NEED_SIM_COMMON=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_AKA)
SRCS+=	eap_server_aka.c
NEED_SIM_COMMON=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_SIM)
SRCS+=	eap_server_sim.c
NEED_SIM_COMMON=y
.endif

.if defined(NEED_SIM_COMMON)
SRCS+=	eap_sim_common.c \
	eap_sim_db.c
NEED_AES_CBC=y
NEED_FIPS186_2_PRF=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
SRCS+=	eap_server_gpsk.c \
	eap_gpsk_common.c
NEED_AES_OMAC1=y
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_PAX)
SRCS+=	eap_server_pax.c \
	eap_pax_common.c
.endif

.if !empty(CFLAGS:M*-DEAP_SERVER_SAKE)
SRCS+=	eap_server_sake.c \
	eap_sake_common.c
.endif

.include "${.CURDIR}/../Makefile.crypto"

.include <bsd.prog.mk>
