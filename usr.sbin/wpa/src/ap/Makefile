.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpaap
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/ap

SRCS=	accounting.c \
	ap_config.c \
	ap_drv_ops.c \
	ap_mlme.c \
	authsrv.c \
	beacon.c \
	bss_load.c \
	ctrl_iface_ap.c \
	dfs.c \
	drv_callbacks.c \
	eap_user_db.c \
	hostapd.c \
	ieee802_11_auth.c \
	ieee802_11_ht.c \
	ieee802_11_shared.c \
	ieee802_11_vht.c \
	ieee802_1x.c \
	mbo_ap.c \
	neighbor_db.c \
	pmksa_cache_auth.c \
	preauth_auth.c \
	rrm.c \
	sta_info.c \
	tkip_countermeasures.c \
	utils.c \
	vlan.c \
	vlan_ifconfig.c \
	vlan_init.c \
	wmm.c \
	wnm_ap.c \
	wpa_auth.c \
	wpa_auth_glue.c \
	wpa_auth_ie.c

.if ${MK_WPA_SUPPLICANT_EAPOL} != "no"
SRCS+=	gas_serv.c \
	hs20.c \
	wps_hostapd.c
.endif

CFLAGS+=-DHOSTAPD

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
