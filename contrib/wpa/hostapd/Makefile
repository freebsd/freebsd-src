ALL=hostapd hostapd_cli
CONFIG_FILE = .config

include ../src/build.rules

ifdef LIBS
# If LIBS is set with some global build system defaults, clone those for
# LIBS_c, LIBS_h, and LIBS_n to cover hostapd_cli, hlr_auc_gw, and
# nt_password_hash as well.
ifndef LIBS_c
LIBS_c := $(LIBS)
endif
ifndef LIBS_h
LIBS_h := $(LIBS)
endif
ifndef LIBS_n
LIBS_n := $(LIBS)
endif
ifndef LIBS_s
LIBS_s := $(LIBS)
endif
endif

CFLAGS += $(EXTRA_CFLAGS)
CFLAGS += -I$(abspath ../src)
CFLAGS += -I$(abspath ../src/utils)

export BINDIR ?= /usr/local/bin/

ifndef CONFIG_NO_GITVER
# Add VERSION_STR postfix for builds from a git repository
ifeq ($(wildcard ../.git),../.git)
GITVER := $(shell git describe --dirty=+)
ifneq ($(GITVER),)
CFLAGS += -DGIT_VERSION_STR_POSTFIX=\"-$(GITVER)\"
endif
endif
endif

ifdef CONFIG_TESTING_OPTIONS
CFLAGS += -DCONFIG_TESTING_OPTIONS
CONFIG_WPS_TESTING=y
endif

ifndef CONFIG_OS
ifdef CONFIG_NATIVE_WINDOWS
CONFIG_OS=win32
else
CONFIG_OS=unix
endif
endif

ifeq ($(CONFIG_OS), internal)
CFLAGS += -DOS_NO_C_LIB_DEFINES
endif

ifdef CONFIG_NATIVE_WINDOWS
CFLAGS += -DCONFIG_NATIVE_WINDOWS
LIBS += -lws2_32
endif

OBJS += main.o
OBJS += config_file.o

OBJS += ../src/ap/hostapd.o
OBJS += ../src/ap/wpa_auth_glue.o
OBJS += ../src/ap/drv_callbacks.o
OBJS += ../src/ap/ap_drv_ops.o
OBJS += ../src/ap/utils.o
OBJS += ../src/ap/authsrv.o
OBJS += ../src/ap/ieee802_1x.o
OBJS += ../src/ap/ap_config.o
OBJS += ../src/ap/eap_user_db.o
OBJS += ../src/ap/ieee802_11_auth.o
OBJS += ../src/ap/sta_info.o
OBJS += ../src/ap/wpa_auth.o
OBJS += ../src/ap/tkip_countermeasures.o
OBJS += ../src/ap/ap_mlme.o
OBJS += ../src/ap/wpa_auth_ie.o
OBJS += ../src/ap/preauth_auth.o
OBJS += ../src/ap/pmksa_cache_auth.o
OBJS += ../src/ap/ieee802_11_shared.o
OBJS += ../src/ap/beacon.o
OBJS += ../src/ap/bss_load.o
OBJS += ../src/ap/neighbor_db.o
OBJS += ../src/ap/rrm.o

OBJS_c = hostapd_cli.o
OBJS_c += ../src/common/wpa_ctrl.o
OBJS_c += ../src/utils/os_$(CONFIG_OS).o
OBJS_c += ../src/common/cli.o

NEED_RC4=y
NEED_AES=y
NEED_MD5=y
NEED_SHA1=y

OBJS += ../src/drivers/drivers.o
CFLAGS += -DHOSTAPD

ifdef CONFIG_TAXONOMY
CFLAGS += -DCONFIG_TAXONOMY
OBJS += ../src/ap/taxonomy.o
endif

ifdef CONFIG_MODULE_TESTS
CFLAGS += -DCONFIG_MODULE_TESTS
OBJS += hapd_module_tests.o
endif

ifdef CONFIG_WPA_TRACE
CFLAGS += -DWPA_TRACE
OBJS += ../src/utils/trace.o
HOBJS += ../src/utils/trace.o
LDFLAGS += -rdynamic
CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
CFLAGS += -DPACKAGE="hostapd" -DWPA_TRACE_BFD
LIBS += -lbfd -ldl -liberty -lz
LIBS_c += -lbfd -ldl -liberty -lz
LIBS_h += -lbfd -ldl -liberty -lz
LIBS_n += -lbfd -ldl -liberty -lz
LIBS_s += -lbfd -ldl -liberty -lz
endif
endif

ifndef CONFIG_ELOOP
CONFIG_ELOOP=eloop
endif
OBJS += ../src/utils/$(CONFIG_ELOOP).o
OBJS_c += ../src/utils/$(CONFIG_ELOOP).o

ifeq ($(CONFIG_ELOOP), eloop)
# Using glibc < 2.17 requires -lrt for clock_gettime()
LIBS += -lrt
LIBS_c += -lrt
LIBS_h += -lrt
LIBS_n += -lrt
endif

ifdef CONFIG_ELOOP_POLL
CFLAGS += -DCONFIG_ELOOP_POLL
endif

ifdef CONFIG_ELOOP_EPOLL
CFLAGS += -DCONFIG_ELOOP_EPOLL
endif

ifdef CONFIG_ELOOP_KQUEUE
CFLAGS += -DCONFIG_ELOOP_KQUEUE
endif

OBJS += ../src/utils/common.o
OBJS_c += ../src/utils/common.o
OBJS += ../src/utils/wpa_debug.o
OBJS_c += ../src/utils/wpa_debug.o
OBJS += ../src/utils/wpabuf.o
OBJS += ../src/utils/os_$(CONFIG_OS).o
OBJS += ../src/utils/ip_addr.o
OBJS += ../src/utils/crc32.o

OBJS += ../src/common/ieee802_11_common.o
OBJS += ../src/common/wpa_common.o
OBJS += ../src/common/hw_features_common.o

OBJS += ../src/eapol_auth/eapol_auth_sm.o


ifdef CONFIG_CODE_COVERAGE
CFLAGS += -O0 -fprofile-arcs -ftest-coverage
LIBS += -lgcov
LIBS_c += -lgcov
LIBS_h += -lgcov
LIBS_n += -lgcov
endif

ifndef CONFIG_NO_DUMP_STATE
# define HOSTAPD_DUMP_STATE to include support for dumping internal state
# through control interface commands (undefine it, if you want to save in
# binary size)
CFLAGS += -DHOSTAPD_DUMP_STATE
OBJS += ../src/eapol_auth/eapol_auth_dump.o
endif

ifdef CONFIG_NO_RADIUS
CFLAGS += -DCONFIG_NO_RADIUS
CONFIG_NO_ACCOUNTING=y
else
OBJS += ../src/radius/radius.o
OBJS += ../src/radius/radius_client.o
OBJS += ../src/radius/radius_das.o
endif

ifdef CONFIG_NO_ACCOUNTING
CFLAGS += -DCONFIG_NO_ACCOUNTING
else
OBJS += ../src/ap/accounting.o
endif

ifdef CONFIG_NO_VLAN
CFLAGS += -DCONFIG_NO_VLAN
else
OBJS += ../src/ap/vlan_init.o
OBJS += ../src/ap/vlan_ifconfig.o
OBJS += ../src/ap/vlan.o
ifdef CONFIG_FULL_DYNAMIC_VLAN
# Define CONFIG_FULL_DYNAMIC_VLAN to have hostapd manipulate bridges
# and VLAN interfaces for the VLAN feature.
CFLAGS += -DCONFIG_FULL_DYNAMIC_VLAN
OBJS += ../src/ap/vlan_full.o
ifdef CONFIG_VLAN_NETLINK
OBJS += ../src/ap/vlan_util.o
else
OBJS += ../src/ap/vlan_ioctl.o
endif
endif
endif

ifdef CONFIG_NO_CTRL_IFACE
CFLAGS += -DCONFIG_NO_CTRL_IFACE
else
ifeq ($(CONFIG_CTRL_IFACE), udp)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
else
ifeq ($(CONFIG_CTRL_IFACE), udp6)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_IPV6
else
ifeq ($(CONFIG_CTRL_IFACE), udp-remote)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_REMOTE
else
ifeq ($(CONFIG_CTRL_IFACE), udp6-remote)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_REMOTE
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_IPV6
else
CFLAGS += -DCONFIG_CTRL_IFACE_UNIX
endif
endif
endif
endif
OBJS += ../src/common/ctrl_iface_common.o
OBJS += ctrl_iface.o
OBJS += ../src/ap/ctrl_iface_ap.o
endif

ifndef CONFIG_NO_CTRL_IFACE
CFLAGS += -DCONFIG_CTRL_IFACE
endif

ifdef CONFIG_RSN_PREAUTH
CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_HS20
CONFIG_PROXYARP=y
endif

ifdef CONFIG_PROXYARP
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_SUITEB
CFLAGS += -DCONFIG_SUITEB
endif

ifdef CONFIG_SUITEB192
CFLAGS += -DCONFIG_SUITEB192
NEED_SHA384=y
endif

ifdef CONFIG_OCV
CFLAGS += -DCONFIG_OCV
OBJS += ../src/common/ocv.o
endif

ifdef CONFIG_IEEE80211R
CFLAGS += -DCONFIG_IEEE80211R -DCONFIG_IEEE80211R_AP
OBJS += ../src/ap/wpa_auth_ft.o
NEED_AES_UNWRAP=y
NEED_AES_SIV=y
NEED_ETH_P_OUI=y
NEED_HMAC_SHA256_KDF=y
endif

ifdef NEED_ETH_P_OUI
CFLAGS += -DCONFIG_ETH_P_OUI
OBJS += ../src/ap/eth_p_oui.o
endif

ifdef CONFIG_SAE
CFLAGS += -DCONFIG_SAE
OBJS += ../src/common/sae.o
ifdef CONFIG_SAE_PK
CFLAGS += -DCONFIG_SAE_PK
OBJS += ../src/common/sae_pk.o
endif
NEED_ECC=y
NEED_DH_GROUPS=y
NEED_HMAC_SHA256_KDF=y
NEED_AP_MLME=y
NEED_DRAGONFLY=y
endif

ifdef CONFIG_OWE
CFLAGS += -DCONFIG_OWE
NEED_ECC=y
NEED_HMAC_SHA256_KDF=y
NEED_HMAC_SHA384_KDF=y
NEED_HMAC_SHA512_KDF=y
NEED_SHA384=y
NEED_SHA512=y
endif

ifdef CONFIG_AIRTIME_POLICY
CFLAGS += -DCONFIG_AIRTIME_POLICY
OBJS += ../src/ap/airtime_policy.o
endif

ifdef CONFIG_FILS
CFLAGS += -DCONFIG_FILS
OBJS += ../src/ap/fils_hlp.o
NEED_SHA384=y
NEED_AES_SIV=y
ifdef CONFIG_FILS_SK_PFS
CFLAGS += -DCONFIG_FILS_SK_PFS
NEED_ECC=y
endif
endif

ifdef CONFIG_WNM
CFLAGS += -DCONFIG_WNM -DCONFIG_WNM_AP
OBJS += ../src/ap/wnm_ap.o
endif

ifdef CONFIG_IEEE80211AC
CFLAGS += -DCONFIG_IEEE80211AC
endif

ifdef CONFIG_IEEE80211AX
CFLAGS += -DCONFIG_IEEE80211AX
OBJS += ../src/ap/ieee802_11_he.o
endif

ifdef CONFIG_MBO
CFLAGS += -DCONFIG_MBO
OBJS += ../src/ap/mbo_ap.o
endif

include ../src/drivers/drivers.mak
OBJS += $(DRV_AP_OBJS)
CFLAGS += $(DRV_AP_CFLAGS)
LDFLAGS += $(DRV_AP_LDFLAGS)
LIBS += $(DRV_AP_LIBS)

ifdef CONFIG_L2_PACKET
ifdef CONFIG_DNET_PCAP
ifdef CONFIG_L2_FREEBSD
LIBS += -lpcap
OBJS += ../src/l2_packet/l2_packet_freebsd.o
else
LIBS += -ldnet -lpcap
OBJS += ../src/l2_packet/l2_packet_pcap.o
endif
else
OBJS += ../src/l2_packet/l2_packet_linux.o
endif
else
OBJS += ../src/l2_packet/l2_packet_none.o
endif


ifdef CONFIG_ERP
CFLAGS += -DCONFIG_ERP
NEED_HMAC_SHA256_KDF=y
endif

ifdef CONFIG_EAP_MD5
CFLAGS += -DEAP_SERVER_MD5
OBJS += ../src/eap_server/eap_server_md5.o
CHAP=y
endif

ifdef CONFIG_EAP_TLS
CFLAGS += -DEAP_SERVER_TLS
OBJS += ../src/eap_server/eap_server_tls.o
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_UNAUTH_TLS
CFLAGS += -DEAP_SERVER_UNAUTH_TLS
ifndef CONFIG_EAP_TLS
OBJS += ../src/eap_server/eap_server_tls.o
TLS_FUNCS=y
endif
endif

ifdef CONFIG_EAP_PEAP
CFLAGS += -DEAP_SERVER_PEAP
OBJS += ../src/eap_server/eap_server_peap.o
OBJS += ../src/eap_common/eap_peap_common.o
TLS_FUNCS=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
CFLAGS += -DEAP_SERVER_TTLS
OBJS += ../src/eap_server/eap_server_ttls.o
TLS_FUNCS=y
CHAP=y
endif

ifdef CONFIG_EAP_MSCHAPV2
CFLAGS += -DEAP_SERVER_MSCHAPV2
OBJS += ../src/eap_server/eap_server_mschapv2.o
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
CFLAGS += -DEAP_SERVER_GTC
OBJS += ../src/eap_server/eap_server_gtc.o
endif

ifdef CONFIG_EAP_SIM
CFLAGS += -DEAP_SERVER_SIM
OBJS += ../src/eap_server/eap_server_sim.o
CONFIG_EAP_SIM_COMMON=y
NEED_AES_CBC=y
endif

ifdef CONFIG_EAP_AKA
CFLAGS += -DEAP_SERVER_AKA
OBJS += ../src/eap_server/eap_server_aka.o
CONFIG_EAP_SIM_COMMON=y
NEED_AES_CBC=y
endif

ifdef CONFIG_EAP_AKA_PRIME
CFLAGS += -DEAP_SERVER_AKA_PRIME
endif

ifdef CONFIG_EAP_SIM_COMMON
OBJS += ../src/eap_common/eap_sim_common.o
# Example EAP-SIM/AKA interface for GSM/UMTS authentication. This can be
# replaced with another file implementing the interface specified in
# eap_sim_db.h.
OBJS += ../src/eap_server/eap_sim_db.o
NEED_FIPS186_2_PRF=y
endif

ifdef CONFIG_EAP_PAX
CFLAGS += -DEAP_SERVER_PAX
OBJS += ../src/eap_server/eap_server_pax.o ../src/eap_common/eap_pax_common.o
endif

ifdef CONFIG_EAP_PSK
CFLAGS += -DEAP_SERVER_PSK
OBJS += ../src/eap_server/eap_server_psk.o ../src/eap_common/eap_psk_common.o
NEED_AES_ENCBLOCK=y
NEED_AES_EAX=y
endif

ifdef CONFIG_EAP_SAKE
CFLAGS += -DEAP_SERVER_SAKE
OBJS += ../src/eap_server/eap_server_sake.o ../src/eap_common/eap_sake_common.o
endif

ifdef CONFIG_EAP_GPSK
CFLAGS += -DEAP_SERVER_GPSK
OBJS += ../src/eap_server/eap_server_gpsk.o ../src/eap_common/eap_gpsk_common.o
ifdef CONFIG_EAP_GPSK_SHA256
CFLAGS += -DEAP_GPSK_SHA256
endif
endif

ifdef CONFIG_EAP_PWD
CFLAGS += -DEAP_SERVER_PWD
OBJS += ../src/eap_server/eap_server_pwd.o ../src/eap_common/eap_pwd_common.o
NEED_ECC=y
NEED_DRAGONFLY=y
endif

ifdef CONFIG_EAP_EKE
CFLAGS += -DEAP_SERVER_EKE
OBJS += ../src/eap_server/eap_server_eke.o ../src/eap_common/eap_eke_common.o
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
endif

ifdef CONFIG_EAP_VENDOR_TEST
CFLAGS += -DEAP_SERVER_VENDOR_TEST
OBJS += ../src/eap_server/eap_server_vendor_test.o
endif

ifdef CONFIG_EAP_FAST
CFLAGS += -DEAP_SERVER_FAST
OBJS += ../src/eap_server/eap_server_fast.o
OBJS += ../src/eap_common/eap_fast_common.o
TLS_FUNCS=y
NEED_T_PRF=y
NEED_AES_UNWRAP=y
endif

ifdef CONFIG_EAP_TEAP
CFLAGS += -DEAP_SERVER_TEAP
OBJS += ../src/eap_server/eap_server_teap.o
OBJS += ../src/eap_common/eap_teap_common.o
TLS_FUNCS=y
NEED_T_PRF=y
NEED_SHA384=y
NEED_TLS_PRF_SHA256=y
NEED_TLS_PRF_SHA384=y
NEED_AES_UNWRAP=y
endif

ifdef CONFIG_WPS
CFLAGS += -DCONFIG_WPS -DEAP_SERVER_WSC
OBJS += ../src/utils/uuid.o
OBJS += ../src/ap/wps_hostapd.o
OBJS += ../src/eap_server/eap_server_wsc.o ../src/eap_common/eap_wsc_common.o
OBJS += ../src/wps/wps.o
OBJS += ../src/wps/wps_common.o
OBJS += ../src/wps/wps_attr_parse.o
OBJS += ../src/wps/wps_attr_build.o
OBJS += ../src/wps/wps_attr_process.o
OBJS += ../src/wps/wps_dev_attr.o
OBJS += ../src/wps/wps_enrollee.o
OBJS += ../src/wps/wps_registrar.o
NEED_DH_GROUPS=y
NEED_BASE64=y
NEED_AES_CBC=y
NEED_MODEXP=y
CONFIG_EAP=y

ifdef CONFIG_WPS_NFC
CFLAGS += -DCONFIG_WPS_NFC
OBJS += ../src/wps/ndef.o
NEED_WPS_OOB=y
endif

ifdef NEED_WPS_OOB
CFLAGS += -DCONFIG_WPS_OOB
endif

ifdef CONFIG_WPS_UPNP
CFLAGS += -DCONFIG_WPS_UPNP
OBJS += ../src/wps/wps_upnp.o
OBJS += ../src/wps/wps_upnp_ssdp.o
OBJS += ../src/wps/wps_upnp_web.o
OBJS += ../src/wps/wps_upnp_event.o
OBJS += ../src/wps/wps_upnp_ap.o
OBJS += ../src/wps/upnp_xml.o
OBJS += ../src/wps/httpread.o
OBJS += ../src/wps/http_client.o
OBJS += ../src/wps/http_server.o
endif

ifdef CONFIG_WPS_STRICT
CFLAGS += -DCONFIG_WPS_STRICT
OBJS += ../src/wps/wps_validate.o
endif

ifdef CONFIG_WPS_TESTING
CFLAGS += -DCONFIG_WPS_TESTING
endif

endif

ifdef CONFIG_DPP
CFLAGS += -DCONFIG_DPP
OBJS += ../src/common/dpp.o
OBJS += ../src/common/dpp_auth.o
OBJS += ../src/common/dpp_backup.o
OBJS += ../src/common/dpp_crypto.o
OBJS += ../src/common/dpp_pkex.o
OBJS += ../src/common/dpp_reconfig.o
OBJS += ../src/common/dpp_tcp.o
OBJS += ../src/ap/dpp_hostapd.o
OBJS += ../src/ap/gas_query_ap.o
NEED_AES_SIV=y
NEED_HMAC_SHA256_KDF=y
NEED_HMAC_SHA384_KDF=y
NEED_HMAC_SHA512_KDF=y
NEED_SHA384=y
NEED_SHA512=y
NEED_ECC=y
NEED_JSON=y
NEED_GAS=y
NEED_BASE64=y
NEED_ASN1=y
ifdef CONFIG_DPP2
CFLAGS += -DCONFIG_DPP2
endif
endif

ifdef CONFIG_PASN
CFLAGS += -DCONFIG_PASN
CFLAGS += -DCONFIG_PTKSA_CACHE
NEED_HMAC_SHA256_KDF=y
NEED_HMAC_SHA384_KDF=y
NEED_SHA256=y
NEED_SHA384=y
OBJS += ../src/common/ptksa_cache.o
endif

ifdef CONFIG_EAP_IKEV2
CFLAGS += -DEAP_SERVER_IKEV2
OBJS += ../src/eap_server/eap_server_ikev2.o ../src/eap_server/ikev2.o
OBJS += ../src/eap_common/eap_ikev2_common.o ../src/eap_common/ikev2_common.o
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
NEED_MODEXP=y
NEED_CIPHER=y
endif

ifdef CONFIG_EAP_TNC
CFLAGS += -DEAP_SERVER_TNC
OBJS += ../src/eap_server/eap_server_tnc.o
OBJS += ../src/eap_server/tncs.o
NEED_BASE64=y
ifndef CONFIG_DRIVER_BSD
LIBS += -ldl
endif
endif

ifdef CONFIG_MACSEC
CFLAGS += -DCONFIG_MACSEC
OBJS += ../src/ap/wpa_auth_kay.o
OBJS += ../src/pae/ieee802_1x_cp.o
OBJS += ../src/pae/ieee802_1x_kay.o
OBJS += ../src/pae/ieee802_1x_key.o
OBJS += ../src/pae/ieee802_1x_secy_ops.o
endif

# Basic EAP functionality is needed for EAPOL
OBJS += eap_register.o
OBJS += ../src/eap_server/eap_server.o
OBJS += ../src/eap_common/eap_common.o
OBJS += ../src/eap_server/eap_server_methods.o
OBJS += ../src/eap_server/eap_server_identity.o
CFLAGS += -DEAP_SERVER_IDENTITY

ifdef CONFIG_EAP
CFLAGS += -DEAP_SERVER
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef NEED_DRAGONFLY
OBJS += ../src/common/dragonfly.o
endif

ifdef MS_FUNCS
OBJS += ../src/crypto/ms_funcs.o
NEED_DES=y
NEED_MD4=y
endif

ifdef CHAP
OBJS += ../src/eap_common/chap.o
endif

ifdef TLS_FUNCS
NEED_DES=y
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
CFLAGS += -DEAP_TLS_FUNCS
OBJS += ../src/eap_server/eap_server_tls_common.o
NEED_TLS_PRF=y
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifdef CONFIG_TLSV11
CFLAGS += -DCONFIG_TLSV11
endif

ifdef CONFIG_TLSV12
CFLAGS += -DCONFIG_TLSV12
endif

ifeq ($(CONFIG_TLS), wolfssl)
CONFIG_CRYPTO=wolfssl
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_wolfssl.o
LIBS += -lwolfssl -lm
endif
OBJS += ../src/crypto/crypto_wolfssl.o
HOBJS += ../src/crypto/crypto_wolfssl.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_wolfssl.o
endif
NEED_TLS_PRF_SHA256=y
LIBS += -lwolfssl -lm
LIBS_h += -lwolfssl -lm
ifdef CONFIG_TLS_ADD_DL
LIBS += -ldl
LIBS_h += -ldl
endif
endif

ifeq ($(CONFIG_TLS), openssl)
CONFIG_CRYPTO=openssl
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_openssl.o
OBJS += ../src/crypto/tls_openssl_ocsp.o
LIBS += -lssl
endif
OBJS += ../src/crypto/crypto_openssl.o
HOBJS += ../src/crypto/crypto_openssl.o
SOBJS += ../src/crypto/crypto_openssl.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_openssl.o
endif
NEED_TLS_PRF_SHA256=y
LIBS += -lcrypto
LIBS_h += -lcrypto
LIBS_n += -lcrypto
LIBS_s += -lcrypto
ifdef CONFIG_TLS_ADD_DL
LIBS += -ldl
LIBS_h += -ldl
LIBS_s += -ldl
endif
ifndef CONFIG_TLS_DEFAULT_CIPHERS
CONFIG_TLS_DEFAULT_CIPHERS = "DEFAULT:!EXP:!LOW"
endif
CFLAGS += -DTLS_DEFAULT_CIPHERS=\"$(CONFIG_TLS_DEFAULT_CIPHERS)\"
endif

ifeq ($(CONFIG_TLS), gnutls)
ifndef CONFIG_CRYPTO
# default to libgcrypt
CONFIG_CRYPTO=gnutls
endif
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_gnutls.o
LIBS += -lgnutls -lgpg-error
endif
OBJS += ../src/crypto/crypto_$(CONFIG_CRYPTO).o
HOBJS += ../src/crypto/crypto_$(CONFIG_CRYPTO).o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_internal.o
SHA1OBJS += ../src/crypto/sha1-internal.o
endif
ifeq ($(CONFIG_CRYPTO), gnutls)
LIBS += -lgcrypt
LIBS_h += -lgcrypt
LIBS_n += -lgcrypt
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif
ifeq ($(CONFIG_CRYPTO), nettle)
LIBS += -lnettle -lgmp
LIBS_p += -lnettle -lgmp
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif
endif

ifeq ($(CONFIG_TLS), internal)
ifndef CONFIG_CRYPTO
CONFIG_CRYPTO=internal
endif
ifdef TLS_FUNCS
OBJS += ../src/crypto/crypto_internal-rsa.o
OBJS += ../src/crypto/tls_internal.o
OBJS += ../src/tls/tlsv1_common.o
OBJS += ../src/tls/tlsv1_record.o
OBJS += ../src/tls/tlsv1_cred.o
OBJS += ../src/tls/tlsv1_server.o
OBJS += ../src/tls/tlsv1_server_write.o
OBJS += ../src/tls/tlsv1_server_read.o
OBJS += ../src/tls/rsa.o
OBJS += ../src/tls/x509v3.o
OBJS += ../src/tls/pkcs1.o
OBJS += ../src/tls/pkcs5.o
OBJS += ../src/tls/pkcs8.o
NEED_ASN1=y
NEED_BASE64=y
NEED_TLS_PRF=y
ifdef CONFIG_TLSV12
NEED_TLS_PRF_SHA256=y
endif
NEED_MODEXP=y
NEED_CIPHER=y
CFLAGS += -DCONFIG_TLS_INTERNAL
CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER
endif
ifdef NEED_CIPHER
NEED_DES=y
OBJS += ../src/crypto/crypto_internal-cipher.o
endif
ifdef NEED_MODEXP
OBJS += ../src/crypto/crypto_internal-modexp.o
OBJS += ../src/tls/bignum.o
endif
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
OBJS += ../src/crypto/crypto_libtomcrypt.o
LIBS += -ltomcrypt -ltfm
LIBS_h += -ltomcrypt -ltfm
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif
ifeq ($(CONFIG_CRYPTO), internal)
OBJS += ../src/crypto/crypto_internal.o
NEED_AES_DEC=y
CFLAGS += -DCONFIG_CRYPTO_INTERNAL
ifdef CONFIG_INTERNAL_LIBTOMMATH
CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
ifdef CONFIG_INTERNAL_LIBTOMMATH_FAST
CFLAGS += -DLTM_FAST
endif
else
LIBS += -ltommath
LIBS_h += -ltommath
endif
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_DES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD4=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_SHA384=y
CONFIG_INTERNAL_SHA512=y
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif
ifeq ($(CONFIG_CRYPTO), cryptoapi)
OBJS += ../src/crypto/crypto_cryptoapi.o
OBJS_p += ../src/crypto/crypto_cryptoapi.o
CFLAGS += -DCONFIG_CRYPTO_CRYPTOAPI
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
endif
endif

ifeq ($(CONFIG_TLS), linux)
OBJS += ../src/crypto/crypto_linux.o
ifdef TLS_FUNCS
OBJS += ../src/crypto/crypto_internal-rsa.o
OBJS += ../src/crypto/tls_internal.o
OBJS += ../src/tls/tlsv1_common.o
OBJS += ../src/tls/tlsv1_record.o
OBJS += ../src/tls/tlsv1_cred.o
OBJS += ../src/tls/tlsv1_server.o
OBJS += ../src/tls/tlsv1_server_write.o
OBJS += ../src/tls/tlsv1_server_read.o
OBJS += ../src/tls/rsa.o
OBJS += ../src/tls/x509v3.o
OBJS += ../src/tls/pkcs1.o
OBJS += ../src/tls/pkcs5.o
OBJS += ../src/tls/pkcs8.o
NEED_ASN1=y
NEED_BASE64=y
NEED_TLS_PRF=y
ifdef CONFIG_TLSV12
NEED_TLS_PRF_SHA256=y
endif
NEED_MODEXP=y
NEED_CIPHER=y
CFLAGS += -DCONFIG_TLS_INTERNAL
CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER
endif
ifdef NEED_MODEXP
OBJS += ../src/crypto/crypto_internal-modexp.o
OBJS += ../src/tls/bignum.o
CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
CFLAGS += -DLTM_FAST
endif
CONFIG_INTERNAL_DH_GROUP5=y
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_internal.o
OBJS += ../src/crypto/sha1-internal.o
endif
endif

ifeq ($(CONFIG_TLS), none)
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_none.o
CFLAGS += -DEAP_TLS_NONE
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD5=y
endif
OBJS += ../src/crypto/crypto_none.o
OBJS_p += ../src/crypto/crypto_none.o
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
endif

ifndef TLS_FUNCS
OBJS += ../src/crypto/tls_none.o
ifeq ($(CONFIG_TLS), internal)
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_RC4=y
endif
endif

AESOBJS = # none so far
ifdef CONFIG_INTERNAL_AES
AESOBJS += ../src/crypto/aes-internal.o ../src/crypto/aes-internal-enc.o
endif

ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), wolfssl)
AESOBJS += ../src/crypto/aes-wrap.o
endif
endif
ifdef NEED_AES_EAX
AESOBJS += ../src/crypto/aes-eax.o
NEED_AES_CTR=y
endif
ifdef NEED_AES_SIV
AESOBJS += ../src/crypto/aes-siv.o
NEED_AES_CTR=y
endif
ifdef NEED_AES_CTR
AESOBJS += ../src/crypto/aes-ctr.o
endif
ifdef NEED_AES_ENCBLOCK
AESOBJS += ../src/crypto/aes-encblock.o
endif
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), wolfssl)
AESOBJS += ../src/crypto/aes-omac1.o
endif
endif
ifdef NEED_AES_UNWRAP
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), wolfssl)
NEED_AES_DEC=y
AESOBJS += ../src/crypto/aes-unwrap.o
endif
endif
endif
endif
ifdef NEED_AES_CBC
NEED_AES_DEC=y
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), wolfssl)
AESOBJS += ../src/crypto/aes-cbc.o
endif
endif
endif
endif
ifdef NEED_AES_DEC
ifdef CONFIG_INTERNAL_AES
AESOBJS += ../src/crypto/aes-internal-dec.o
endif
endif
ifdef NEED_AES
OBJS += $(AESOBJS)
endif

ifdef NEED_SHA1
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), gnutls)
ifneq ($(CONFIG_TLS), wolfssl)
SHA1OBJS += ../src/crypto/sha1.o
endif
endif
endif
endif
SHA1OBJS += ../src/crypto/sha1-prf.o
ifdef CONFIG_INTERNAL_SHA1
SHA1OBJS += ../src/crypto/sha1-internal.o
ifdef NEED_FIPS186_2_PRF
SHA1OBJS += ../src/crypto/fips_prf_internal.o
endif
endif
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), wolfssl)
SHA1OBJS += ../src/crypto/sha1-pbkdf2.o
endif
endif
ifdef NEED_T_PRF
SHA1OBJS += ../src/crypto/sha1-tprf.o
endif
ifdef NEED_TLS_PRF
SHA1OBJS += ../src/crypto/sha1-tlsprf.o
endif
endif

ifdef NEED_SHA1
OBJS += $(SHA1OBJS)
endif

ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), gnutls)
ifneq ($(CONFIG_TLS), wolfssl)
OBJS += ../src/crypto/md5.o
endif
endif
endif
endif

ifdef NEED_MD5
ifdef CONFIG_INTERNAL_MD5
OBJS += ../src/crypto/md5-internal.o
HOBJS += ../src/crypto/md5-internal.o
endif
endif

ifdef NEED_MD4
ifdef CONFIG_INTERNAL_MD4
OBJS += ../src/crypto/md4-internal.o
endif
endif

ifdef NEED_DES
CFLAGS += -DCONFIG_DES
ifdef CONFIG_INTERNAL_DES
OBJS += ../src/crypto/des-internal.o
endif
endif

ifdef CONFIG_NO_RC4
CFLAGS += -DCONFIG_NO_RC4
endif

ifdef NEED_RC4
ifdef CONFIG_INTERNAL_RC4
ifndef CONFIG_NO_RC4
OBJS += ../src/crypto/rc4.o
endif
endif
endif

CFLAGS += -DCONFIG_SHA256
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), gnutls)
ifneq ($(CONFIG_TLS), wolfssl)
OBJS += ../src/crypto/sha256.o
endif
endif
endif
endif
OBJS += ../src/crypto/sha256-prf.o
ifdef CONFIG_INTERNAL_SHA256
OBJS += ../src/crypto/sha256-internal.o
endif
ifdef NEED_TLS_PRF_SHA256
OBJS += ../src/crypto/sha256-tlsprf.o
endif
ifdef NEED_TLS_PRF_SHA384
OBJS += ../src/crypto/sha384-tlsprf.o
endif
ifdef NEED_HMAC_SHA256_KDF
OBJS += ../src/crypto/sha256-kdf.o
endif
ifdef NEED_HMAC_SHA384_KDF
OBJS += ../src/crypto/sha384-kdf.o
endif
ifdef NEED_HMAC_SHA512_KDF
OBJS += ../src/crypto/sha512-kdf.o
endif
ifdef NEED_SHA384
CFLAGS += -DCONFIG_SHA384
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), gnutls)
ifneq ($(CONFIG_TLS), wolfssl)
OBJS += ../src/crypto/sha384.o
endif
endif
endif
endif
OBJS += ../src/crypto/sha384-prf.o
endif
ifdef NEED_SHA512
CFLAGS += -DCONFIG_SHA512
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), gnutls)
ifneq ($(CONFIG_TLS), wolfssl)
OBJS += ../src/crypto/sha512.o
endif
endif
endif
endif
OBJS += ../src/crypto/sha512-prf.o
endif

ifdef CONFIG_INTERNAL_SHA384
CFLAGS += -DCONFIG_INTERNAL_SHA384
OBJS += ../src/crypto/sha384-internal.o
endif

ifdef CONFIG_INTERNAL_SHA512
CFLAGS += -DCONFIG_INTERNAL_SHA512
OBJS += ../src/crypto/sha512-internal.o
endif

ifdef NEED_ASN1
OBJS += ../src/tls/asn1.o
endif

ifdef NEED_DH_GROUPS
OBJS += ../src/crypto/dh_groups.o
endif
ifdef NEED_DH_GROUPS_ALL
CFLAGS += -DALL_DH_GROUPS
endif
ifdef CONFIG_INTERNAL_DH_GROUP5
ifdef NEED_DH_GROUPS
OBJS += ../src/crypto/dh_group5.o
endif
endif

ifdef NEED_ECC
CFLAGS += -DCONFIG_ECC
endif

ifdef CONFIG_NO_RANDOM_POOL
CFLAGS += -DCONFIG_NO_RANDOM_POOL
else
ifdef CONFIG_GETRANDOM
CFLAGS += -DCONFIG_GETRANDOM
endif
OBJS += ../src/crypto/random.o
HOBJS += ../src/crypto/random.o
HOBJS += ../src/utils/eloop.o
HOBJS += $(SHA1OBJS)
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), wolfssl)
HOBJS += ../src/crypto/md5.o
endif
endif
endif
endif

ifdef CONFIG_RADIUS_SERVER
CFLAGS += -DRADIUS_SERVER
OBJS += ../src/radius/radius_server.o
endif

ifdef CONFIG_IPV6
CFLAGS += -DCONFIG_IPV6
endif

ifdef CONFIG_DRIVER_RADIUS_ACL
CFLAGS += -DCONFIG_DRIVER_RADIUS_ACL
endif

ifdef NEED_BASE64
OBJS += ../src/utils/base64.o
endif

ifdef NEED_JSON
OBJS += ../src/utils/json.o
CFLAGS += -DCONFIG_JSON
endif

ifdef NEED_AP_MLME
OBJS += ../src/ap/wmm.o
OBJS += ../src/ap/ap_list.o
OBJS += ../src/ap/ieee802_11.o
OBJS += ../src/ap/hw_features.o
OBJS += ../src/ap/dfs.o
CFLAGS += -DNEED_AP_MLME
endif
OBJS += ../src/ap/ieee802_11_ht.o

ifdef CONFIG_IEEE80211AC
OBJS += ../src/ap/ieee802_11_vht.o
endif

ifdef CONFIG_P2P_MANAGER
CFLAGS += -DCONFIG_P2P_MANAGER
OBJS += ../src/ap/p2p_hostapd.o
endif

ifdef CONFIG_HS20
CFLAGS += -DCONFIG_HS20
OBJS += ../src/ap/hs20.o
CONFIG_INTERWORKING=y
endif

ifdef CONFIG_INTERWORKING
CFLAGS += -DCONFIG_INTERWORKING
NEED_GAS=y
endif

ifdef NEED_GAS
OBJS += ../src/common/gas.o
OBJS += ../src/ap/gas_serv.o
endif

ifdef CONFIG_PROXYARP
CFLAGS += -DCONFIG_PROXYARP
OBJS += ../src/ap/x_snoop.o
OBJS += ../src/ap/dhcp_snoop.o
ifdef CONFIG_IPV6
OBJS += ../src/ap/ndisc_snoop.o
endif
endif

OBJS += ../src/drivers/driver_common.o

ifdef CONFIG_WPA_CLI_EDIT
OBJS_c += ../src/utils/edit.o
else
OBJS_c += ../src/utils/edit_simple.o
endif

ifdef CONFIG_ACS
CFLAGS += -DCONFIG_ACS
OBJS += ../src/ap/acs.o
LIBS += -lm
endif

ifdef CONFIG_NO_STDOUT_DEBUG
CFLAGS += -DCONFIG_NO_STDOUT_DEBUG
endif

ifdef CONFIG_DEBUG_SYSLOG
CFLAGS += -DCONFIG_DEBUG_SYSLOG
endif

ifdef CONFIG_DEBUG_LINUX_TRACING
CFLAGS += -DCONFIG_DEBUG_LINUX_TRACING
endif

ifdef CONFIG_DEBUG_FILE
CFLAGS += -DCONFIG_DEBUG_FILE
endif

ifdef CONFIG_SQLITE
CFLAGS += -DCONFIG_SQLITE
LIBS += -lsqlite3
LIBS_h += -lsqlite3
endif

ifdef CONFIG_FST
CFLAGS += -DCONFIG_FST
OBJS += ../src/fst/fst.o
OBJS += ../src/fst/fst_group.o
OBJS += ../src/fst/fst_iface.o
OBJS += ../src/fst/fst_session.o
OBJS += ../src/fst/fst_ctrl_aux.o
ifdef CONFIG_FST_TEST
CFLAGS += -DCONFIG_FST_TEST
endif
ifndef CONFIG_NO_CTRL_IFACE
OBJS += ../src/fst/fst_ctrl_iface.o
endif
endif

ifdef CONFIG_WEP
CFLAGS += -DCONFIG_WEP
endif

ifdef CONFIG_NO_TKIP
CFLAGS += -DCONFIG_NO_TKIP
endif

$(DESTDIR)$(BINDIR)/%: %
	install -D $(<) $(@)

install: $(addprefix $(DESTDIR)$(BINDIR)/,$(ALL))

_OBJS_VAR := OBJS
include ../src/objs.mk

hostapd: $(OBJS)
	$(Q)$(CC) $(LDFLAGS) -o hostapd $(OBJS) $(LIBS)
	@$(E) "  LD " $@

ifdef CONFIG_WPA_TRACE
OBJS_c += ../src/utils/trace.o
endif

_OBJS_VAR := OBJS_c
include ../src/objs.mk

hostapd_cli: $(OBJS_c)
	$(Q)$(CC) $(LDFLAGS) -o hostapd_cli $(OBJS_c) $(LIBS_c)
	@$(E) "  LD " $@

NOBJS = nt_password_hash.o ../src/crypto/ms_funcs.o $(SHA1OBJS)
NOBJS += ../src/utils/common.o
ifdef NEED_RC4
ifdef CONFIG_INTERNAL_RC4
ifndef CONFIG_NO_RC4
NOBJS += ../src/crypto/rc4.o
endif
endif
endif
ifdef CONFIG_INTERNAL_MD5
NOBJS += ../src/crypto/md5-internal.o
endif
NOBJS += ../src/crypto/crypto_$(CONFIG_CRYPTO).o
NOBJS += ../src/utils/os_$(CONFIG_OS).o
NOBJS += ../src/utils/wpa_debug.o
NOBJS += ../src/utils/wpabuf.o
ifdef CONFIG_WPA_TRACE
NOBJS += ../src/utils/trace.o
endif

HOBJS += hlr_auc_gw.o ../src/utils/common.o ../src/utils/wpa_debug.o ../src/utils/os_$(CONFIG_OS).o ../src/utils/wpabuf.o ../src/crypto/milenage.o
HOBJS += ../src/crypto/aes-encblock.o
ifdef CONFIG_INTERNAL_AES
HOBJS += ../src/crypto/aes-internal.o
HOBJS += ../src/crypto/aes-internal-enc.o
endif
ifeq ($(CONFIG_TLS), linux)
HOBJS += ../src/crypto/crypto_linux.o
endif

SOBJS += sae_pk_gen.o
SOBJS +=  ../src/utils/common.o
SOBJS += ../src/utils/os_$(CONFIG_OS).o
SOBJS += ../src/utils/base64.o
SOBJS += ../src/utils/wpa_debug.o
SOBJS += ../src/utils/wpabuf.o
ifdef CONFIG_WPA_TRACE
SOBJS += ../src/utils/trace.o
endif
SOBJS += ../src/common/ieee802_11_common.o
SOBJS += ../src/common/sae.o
SOBJS += ../src/common/sae_pk.o
SOBJS += ../src/common/dragonfly.o
SOBJS += $(AESOBJS)
SOBJS += ../src/crypto/sha256-prf.o
SOBJS += ../src/crypto/sha384-prf.o
SOBJS += ../src/crypto/sha512-prf.o
SOBJS += ../src/crypto/dh_groups.o
SOBJS += ../src/crypto/sha256-kdf.o
SOBJS += ../src/crypto/sha384-kdf.o
SOBJS += ../src/crypto/sha512-kdf.o

_OBJS_VAR := NOBJS
include ../src/objs.mk
_OBJS_VAR := HOBJS
include ../src/objs.mk
_OBJS_VAR := SOBJS
include ../src/objs.mk

nt_password_hash: $(NOBJS)
	$(Q)$(CC) $(LDFLAGS) -o nt_password_hash $(NOBJS) $(LIBS_n)
	@$(E) "  LD " $@

hlr_auc_gw: $(HOBJS)
	$(Q)$(CC) $(LDFLAGS) -o hlr_auc_gw $(HOBJS) $(LIBS_h)
	@$(E) "  LD " $@

sae_pk_gen: $(SOBJS)
	$(Q)$(CC) $(LDFLAGS) -o sae_pk_gen $(SOBJS) $(LIBS_s)
	@$(E) "  LD " $@

.PHONY: lcov-html
lcov-html:
	lcov -c -d $(BUILDDIR) > lcov.info
	genhtml lcov.info --output-directory lcov-html

clean: common-clean
	rm -f core *~ nt_password_hash hlr_auc_gw
	rm -f sae_pk_gen
	rm -f lcov.info
	rm -rf lcov-html
