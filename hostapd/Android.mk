# Copyright (C) 2008 The Android Open Source Project
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.
#

LOCAL_PATH := $(call my-dir)

WPA_BUILD_HOSTAPD := false
ifneq ($(BOARD_HOSTAPD_DRIVER),)
  WPA_BUILD_HOSTAPD := true
  CONFIG_DRIVER_$(BOARD_HOSTAPD_DRIVER) := y
endif

ifeq ($(WPA_BUILD_HOSTAPD),true)

include $(LOCAL_PATH)/android.config

# To ignore possible wrong network configurations
L_CFLAGS = -DWPA_IGNORE_CONFIG_ERRORS

L_CFLAGS += -DVERSION_STR_POSTFIX=\"-$(PLATFORM_VERSION)\"

# Set Android log name
L_CFLAGS += -DANDROID_LOG_NAME=\"hostapd\"

# Disable unused parameter warnings
L_CFLAGS += -Wno-unused-parameter

# Set Android extended P2P functionality
L_CFLAGS += -DANDROID_P2P

ifeq ($(BOARD_HOSTAPD_PRIVATE_LIB),)
L_CFLAGS += -DANDROID_LIB_STUB
endif

# Use Android specific directory for control interface sockets
L_CFLAGS += -DCONFIG_CTRL_IFACE_CLIENT_DIR=\"/data/misc/wifi/sockets\"
L_CFLAGS += -DCONFIG_CTRL_IFACE_DIR=\"/data/system/hostapd\"

# Use Android specific directory for hostapd_cli command completion history
L_CFLAGS += -DCONFIG_HOSTAPD_CLI_HISTORY_DIR=\"/data/misc/wifi\"

# To force sizeof(enum) = 4
ifeq ($(TARGET_ARCH),arm)
L_CFLAGS += -mabi=aapcs-linux
endif

INCLUDES = $(LOCAL_PATH)
INCLUDES += $(LOCAL_PATH)/src
INCLUDES += $(LOCAL_PATH)/src/utils
INCLUDES += system/security/keystore/include
ifdef CONFIG_DRIVER_NL80211
ifneq ($(wildcard external/libnl),)
INCLUDES += external/libnl/include
else
INCLUDES += external/libnl-headers
endif
endif


ifndef CONFIG_OS
ifdef CONFIG_NATIVE_WINDOWS
CONFIG_OS=win32
else
CONFIG_OS=unix
endif
endif

ifeq ($(CONFIG_OS), internal)
L_CFLAGS += -DOS_NO_C_LIB_DEFINES
endif

ifdef CONFIG_NATIVE_WINDOWS
L_CFLAGS += -DCONFIG_NATIVE_WINDOWS
LIBS += -lws2_32
endif

OBJS = main.c
OBJS += config_file.c

OBJS += src/ap/hostapd.c
OBJS += src/ap/wpa_auth_glue.c
OBJS += src/ap/drv_callbacks.c
OBJS += src/ap/ap_drv_ops.c
OBJS += src/ap/utils.c
OBJS += src/ap/authsrv.c
OBJS += src/ap/ieee802_1x.c
OBJS += src/ap/ap_config.c
OBJS += src/ap/eap_user_db.c
OBJS += src/ap/ieee802_11_auth.c
OBJS += src/ap/sta_info.c
OBJS += src/ap/wpa_auth.c
OBJS += src/ap/tkip_countermeasures.c
OBJS += src/ap/ap_mlme.c
OBJS += src/ap/wpa_auth_ie.c
OBJS += src/ap/preauth_auth.c
OBJS += src/ap/pmksa_cache_auth.c
OBJS += src/ap/ieee802_11_shared.c
OBJS += src/ap/beacon.c
OBJS += src/ap/bss_load.c
OBJS += src/ap/neighbor_db.c
OBJS += src/ap/rrm.c
OBJS_d =
OBJS_p =
LIBS =
LIBS_c =
HOBJS =
LIBS_h =

NEED_RC4=y
NEED_AES=y
NEED_MD5=y
NEED_SHA1=y

OBJS += src/drivers/drivers.c
L_CFLAGS += -DHOSTAPD

ifdef CONFIG_WPA_TRACE
L_CFLAGS += -DWPA_TRACE
OBJS += src/utils/trace.c
HOBJS += src/utils/trace.c
LDFLAGS += -rdynamic
L_CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
L_CFLAGS += -DWPA_TRACE_BFD
LIBS += -lbfd
LIBS_c += -lbfd
LIBS_h += -lbfd
endif
endif

OBJS += src/utils/eloop.c

ifdef CONFIG_ELOOP_POLL
L_CFLAGS += -DCONFIG_ELOOP_POLL
endif

ifdef CONFIG_ELOOP_EPOLL
L_CFLAGS += -DCONFIG_ELOOP_EPOLL
endif

OBJS += src/utils/common.c
OBJS += src/utils/wpa_debug.c
OBJS += src/utils/wpabuf.c
OBJS += src/utils/os_$(CONFIG_OS).c
OBJS += src/utils/ip_addr.c

OBJS += src/common/ieee802_11_common.c
OBJS += src/common/wpa_common.c
OBJS += src/common/hw_features_common.c

OBJS += src/eapol_auth/eapol_auth_sm.c


ifndef CONFIG_NO_DUMP_STATE
# define HOSTAPD_DUMP_STATE to include support for dumping internal state
# through control interface commands (undefine it, if you want to save in
# binary size)
L_CFLAGS += -DHOSTAPD_DUMP_STATE
OBJS += src/eapol_auth/eapol_auth_dump.c
endif

ifdef CONFIG_NO_RADIUS
L_CFLAGS += -DCONFIG_NO_RADIUS
CONFIG_NO_ACCOUNTING=y
else
OBJS += src/radius/radius.c
OBJS += src/radius/radius_client.c
OBJS += src/radius/radius_das.c
endif

ifdef CONFIG_NO_ACCOUNTING
L_CFLAGS += -DCONFIG_NO_ACCOUNTING
else
OBJS += src/ap/accounting.c
endif

ifdef CONFIG_NO_VLAN
L_CFLAGS += -DCONFIG_NO_VLAN
else
OBJS += src/ap/vlan_init.c
OBJS += src/ap/vlan_ifconfig.c
OBJS += src/ap/vlan.c
ifdef CONFIG_FULL_DYNAMIC_VLAN
# Define CONFIG_FULL_DYNAMIC_VLAN to have hostapd manipulate bridges
# and VLAN interfaces for the VLAN feature.
L_CFLAGS += -DCONFIG_FULL_DYNAMIC_VLAN
OBJS += src/ap/vlan_full.c
ifdef CONFIG_VLAN_NETLINK
OBJS += src/ap/vlan_util.c
else
OBJS += src/ap/vlan_ioctl.c
endif
endif
endif

ifdef CONFIG_NO_CTRL_IFACE
L_CFLAGS += -DCONFIG_NO_CTRL_IFACE
else
OBJS += src/common/ctrl_iface_common.c
OBJS += ctrl_iface.c
OBJS += src/ap/ctrl_iface_ap.c
endif

L_CFLAGS += -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX

ifdef CONFIG_IAPP
L_CFLAGS += -DCONFIG_IAPP
OBJS += src/ap/iapp.c
endif

ifdef CONFIG_RSN_PREAUTH
L_CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_HS20
NEED_AES_OMAC1=y
CONFIG_PROXYARP=y
endif

ifdef CONFIG_PROXYARP
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_SUITEB
L_CFLAGS += -DCONFIG_SUITEB
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_SUITEB192
L_CFLAGS += -DCONFIG_SUITEB192
NEED_SHA384=y
endif

ifdef CONFIG_OCV
L_CFLAGS += -DCONFIG_OCV
OBJS += src/common/ocv.c
CONFIG_IEEE80211W=y
endif

ifdef CONFIG_IEEE80211W
L_CFLAGS += -DCONFIG_IEEE80211W
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_IEEE80211R
L_CFLAGS += -DCONFIG_IEEE80211R -DCONFIG_IEEE80211R_AP
OBJS += src/ap/wpa_auth_ft.c
NEED_SHA256=y
NEED_AES_OMAC1=y
NEED_AES_UNWRAP=y
NEED_AES_SIV=y
NEED_ETH_P_OUI=y
NEED_SHA256=y
NEED_HMAC_SHA256_KDF=y
endif

ifdef NEED_ETH_P_OUI
L_CFLAGS += -DCONFIG_ETH_P_OUI
OBJS += src/ap/eth_p_oui.c
endif

ifdef CONFIG_SAE
L_CFLAGS += -DCONFIG_SAE
OBJS += src/common/sae.c
NEED_ECC=y
NEED_DH_GROUPS=y
NEED_DRAGONFLY=y
endif

ifdef CONFIG_OWE
L_CFLAGS += -DCONFIG_OWE
NEED_ECC=y
NEED_HMAC_SHA256_KDF=y
NEED_HMAC_SHA384_KDF=y
NEED_HMAC_SHA512_KDF=y
NEED_SHA256=y
NEED_SHA384=y
NEED_SHA512=y
endif

ifdef CONFIG_FILS
L_CFLAGS += -DCONFIG_FILS
OBJS += src/ap/fils_hlp.c
NEED_SHA384=y
NEED_AES_SIV=y
ifdef CONFIG_FILS_SK_PFS
L_CFLAGS += -DCONFIG_FILS_SK_PFS
NEED_ECC=y
endif
endif

ifdef CONFIG_WNM
L_CFLAGS += -DCONFIG_WNM -DCONFIG_WNM_AP
OBJS += src/ap/wnm_ap.c
endif

ifdef CONFIG_IEEE80211N
L_CFLAGS += -DCONFIG_IEEE80211N
endif

ifdef CONFIG_IEEE80211AC
L_CFLAGS += -DCONFIG_IEEE80211AC
endif

ifdef CONFIG_IEEE80211AX
L_CFLAGS += -DCONFIG_IEEE80211AX
endif

ifdef CONFIG_MBO
L_CFLAGS += -DCONFIG_MBO
OBJS += src/ap/mbo_ap.c
endif

ifdef CONFIG_FST
L_CFLAGS += -DCONFIG_FST
OBJS += src/fst/fst.c
OBJS += src/fst/fst_group.c
OBJS += src/fst/fst_iface.c
OBJS += src/fst/fst_session.c
OBJS += src/fst/fst_ctrl_aux.c
ifdef CONFIG_FST_TEST
L_CFLAGS += -DCONFIG_FST_TEST
endif
ifndef CONFIG_NO_CTRL_IFACE
OBJS += src/fst/fst_ctrl_iface.c
endif
endif


include $(LOCAL_PATH)/src/drivers/drivers.mk

OBJS += $(DRV_AP_OBJS)
L_CFLAGS += $(DRV_AP_CFLAGS)
LDFLAGS += $(DRV_AP_LDFLAGS)
LIBS += $(DRV_AP_LIBS)

ifdef CONFIG_L2_PACKET
ifdef CONFIG_DNET_PCAP
ifdef CONFIG_L2_FREEBSD
LIBS += -lpcap
OBJS += src/l2_packet/l2_packet_freebsd.c
else
LIBS += -ldnet -lpcap
OBJS += src/l2_packet/l2_packet_pcap.c
endif
else
OBJS += src/l2_packet/l2_packet_linux.c
endif
else
OBJS += src/l2_packet/l2_packet_none.c
endif


ifdef CONFIG_EAP_MD5
L_CFLAGS += -DEAP_SERVER_MD5
OBJS += src/eap_server/eap_server_md5.c
CHAP=y
endif

ifdef CONFIG_EAP_TLS
L_CFLAGS += -DEAP_SERVER_TLS
OBJS += src/eap_server/eap_server_tls.c
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_UNAUTH_TLS
L_CFLAGS += -DEAP_SERVER_UNAUTH_TLS
ifndef CONFIG_EAP_TLS
OBJS += src/eap_server/eap_server_tls.c
TLS_FUNCS=y
endif
endif

ifdef CONFIG_EAP_PEAP
L_CFLAGS += -DEAP_SERVER_PEAP
OBJS += src/eap_server/eap_server_peap.c
OBJS += src/eap_common/eap_peap_common.c
TLS_FUNCS=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
L_CFLAGS += -DEAP_SERVER_TTLS
OBJS += src/eap_server/eap_server_ttls.c
TLS_FUNCS=y
CHAP=y
endif

ifdef CONFIG_EAP_MSCHAPV2
L_CFLAGS += -DEAP_SERVER_MSCHAPV2
OBJS += src/eap_server/eap_server_mschapv2.c
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
L_CFLAGS += -DEAP_SERVER_GTC
OBJS += src/eap_server/eap_server_gtc.c
endif

ifdef CONFIG_EAP_SIM
L_CFLAGS += -DEAP_SERVER_SIM
OBJS += src/eap_server/eap_server_sim.c
CONFIG_EAP_SIM_COMMON=y
NEED_AES_CBC=y
endif

ifdef CONFIG_EAP_AKA
L_CFLAGS += -DEAP_SERVER_AKA
OBJS += src/eap_server/eap_server_aka.c
CONFIG_EAP_SIM_COMMON=y
NEED_SHA256=y
NEED_AES_CBC=y
endif

ifdef CONFIG_EAP_AKA_PRIME
L_CFLAGS += -DEAP_SERVER_AKA_PRIME
endif

ifdef CONFIG_EAP_SIM_COMMON
OBJS += src/eap_common/eap_sim_common.c
# Example EAP-SIM/AKA interface for GSM/UMTS authentication. This can be
# replaced with another file implementating the interface specified in
# eap_sim_db.h.
OBJS += src/eap_server/eap_sim_db.c
NEED_FIPS186_2_PRF=y
endif

ifdef CONFIG_EAP_PAX
L_CFLAGS += -DEAP_SERVER_PAX
OBJS += src/eap_server/eap_server_pax.c src/eap_common/eap_pax_common.c
endif

ifdef CONFIG_EAP_PSK
L_CFLAGS += -DEAP_SERVER_PSK
OBJS += src/eap_server/eap_server_psk.c src/eap_common/eap_psk_common.c
NEED_AES_OMAC1=y
NEED_AES_ENCBLOCK=y
NEED_AES_EAX=y
endif

ifdef CONFIG_EAP_SAKE
L_CFLAGS += -DEAP_SERVER_SAKE
OBJS += src/eap_server/eap_server_sake.c src/eap_common/eap_sake_common.c
endif

ifdef CONFIG_EAP_GPSK
L_CFLAGS += -DEAP_SERVER_GPSK
OBJS += src/eap_server/eap_server_gpsk.c src/eap_common/eap_gpsk_common.c
ifdef CONFIG_EAP_GPSK_SHA256
L_CFLAGS += -DEAP_GPSK_SHA256
endif
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_EAP_PWD
L_CFLAGS += -DEAP_SERVER_PWD
OBJS += src/eap_server/eap_server_pwd.c src/eap_common/eap_pwd_common.c
NEED_SHA256=y
NEED_ECC=y
NEED_DRAGONFLY=y
endif

ifdef CONFIG_EAP_EKE
L_CFLAGS += -DEAP_SERVER_EKE
OBJS += src/eap_server/eap_server_eke.c src/eap_common/eap_eke_common.c
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
endif

ifdef CONFIG_EAP_VENDOR_TEST
L_CFLAGS += -DEAP_SERVER_VENDOR_TEST
OBJS += src/eap_server/eap_server_vendor_test.c
endif

ifdef CONFIG_EAP_FAST
L_CFLAGS += -DEAP_SERVER_FAST
OBJS += src/eap_server/eap_server_fast.c
OBJS += src/eap_common/eap_fast_common.c
TLS_FUNCS=y
NEED_T_PRF=y
NEED_AES_UNWRAP=y
endif

ifdef CONFIG_EAP_TEAP
L_CFLAGS += -DEAP_SERVER_TEAP
OBJS += src/eap_server/eap_server_teap.c
OBJS += src/eap_common/eap_teap_common.c
TLS_FUNCS=y
NEED_T_PRF=y
NEED_SHA384=y
NEED_AES_UNWRAP=y
endif

ifdef CONFIG_WPS
L_CFLAGS += -DCONFIG_WPS -DEAP_SERVER_WSC
OBJS += src/utils/uuid.c
OBJS += src/ap/wps_hostapd.c
OBJS += src/eap_server/eap_server_wsc.c src/eap_common/eap_wsc_common.c
OBJS += src/wps/wps.c
OBJS += src/wps/wps_common.c
OBJS += src/wps/wps_attr_parse.c
OBJS += src/wps/wps_attr_build.c
OBJS += src/wps/wps_attr_process.c
OBJS += src/wps/wps_dev_attr.c
OBJS += src/wps/wps_enrollee.c
OBJS += src/wps/wps_registrar.c
NEED_DH_GROUPS=y
NEED_SHA256=y
NEED_BASE64=y
NEED_AES_CBC=y
NEED_MODEXP=y
CONFIG_EAP=y

ifdef CONFIG_WPS_NFC
L_CFLAGS += -DCONFIG_WPS_NFC
OBJS += src/wps/ndef.c
NEED_WPS_OOB=y
endif

ifdef NEED_WPS_OOB
L_CFLAGS += -DCONFIG_WPS_OOB
endif

ifdef CONFIG_WPS_UPNP
L_CFLAGS += -DCONFIG_WPS_UPNP
OBJS += src/wps/wps_upnp.c
OBJS += src/wps/wps_upnp_ssdp.c
OBJS += src/wps/wps_upnp_web.c
OBJS += src/wps/wps_upnp_event.c
OBJS += src/wps/wps_upnp_ap.c
OBJS += src/wps/upnp_xml.c
OBJS += src/wps/httpread.c
OBJS += src/wps/http_client.c
OBJS += src/wps/http_server.c
endif

ifdef CONFIG_WPS_STRICT
L_CFLAGS += -DCONFIG_WPS_STRICT
OBJS += src/wps/wps_validate.c
endif

ifdef CONFIG_WPS_TESTING
L_CFLAGS += -DCONFIG_WPS_TESTING
endif

endif

ifdef CONFIG_DPP
L_CFLAGS += -DCONFIG_DPP
OBJS += src/common/dpp.c
OBJS += src/ap/dpp_hostapd.c
OBJS += src/ap/gas_query_ap.c
NEED_AES_SIV=y
NEED_HMAC_SHA256_KDF=y
NEED_HMAC_SHA384_KDF=y
NEED_HMAC_SHA512_KDF=y
NEED_SHA256=y
NEED_SHA384=y
NEED_SHA512=y
NEED_JSON=y
NEED_GAS=y
NEED_BASE64=y
ifdef CONFIG_DPP2
L_CFLAGS += -DCONFIG_DPP2
endif
endif

ifdef CONFIG_EAP_IKEV2
L_CFLAGS += -DEAP_SERVER_IKEV2
OBJS += src/eap_server/eap_server_ikev2.c src/eap_server/ikev2.c
OBJS += src/eap_common/eap_ikev2_common.c src/eap_common/ikev2_common.c
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
NEED_MODEXP=y
NEED_CIPHER=y
endif

ifdef CONFIG_EAP_TNC
L_CFLAGS += -DEAP_SERVER_TNC
OBJS += src/eap_server/eap_server_tnc.c
OBJS += src/eap_server/tncs.c
NEED_BASE64=y
ifndef CONFIG_DRIVER_BSD
LIBS += -ldl
endif
endif

# Basic EAP functionality is needed for EAPOL
OBJS += eap_register.c
OBJS += src/eap_server/eap_server.c
OBJS += src/eap_common/eap_common.c
OBJS += src/eap_server/eap_server_methods.c
OBJS += src/eap_server/eap_server_identity.c
L_CFLAGS += -DEAP_SERVER_IDENTITY

ifdef CONFIG_EAP
L_CFLAGS += -DEAP_SERVER
endif

ifdef CONFIG_PKCS12
L_CFLAGS += -DPKCS12_FUNCS
endif

ifdef NEED_DRAGONFLY
OBJS += src/common/dragonfly.c
endif

ifdef MS_FUNCS
OBJS += src/crypto/ms_funcs.c
NEED_DES=y
NEED_MD4=y
endif

ifdef CHAP
OBJS += src/eap_common/chap.c
endif

ifdef TLS_FUNCS
NEED_DES=y
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
L_CFLAGS += -DEAP_TLS_FUNCS
OBJS += src/eap_server/eap_server_tls_common.c
NEED_TLS_PRF=y
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifdef CONFIG_TLSV11
L_CFLAGS += -DCONFIG_TLSV11
endif

ifdef CONFIG_TLSV12
L_CFLAGS += -DCONFIG_TLSV12
NEED_SHA256=y
endif

ifeq ($(CONFIG_TLS), openssl)
ifdef TLS_FUNCS
OBJS += src/crypto/tls_openssl.c
OBJS += src/crypto/tls_openssl_ocsp.c
LIBS += -lssl
endif
OBJS += src/crypto/crypto_openssl.c
HOBJS += src/crypto/crypto_openssl.c
ifdef NEED_FIPS186_2_PRF
OBJS += src/crypto/fips_prf_openssl.c
endif
NEED_SHA256=y
NEED_TLS_PRF_SHA256=y
LIBS += -lcrypto
LIBS_h += -lcrypto
ifndef CONFIG_TLS_DEFAULT_CIPHERS
CONFIG_TLS_DEFAULT_CIPHERS = "DEFAULT:!EXP:!LOW"
endif
L_CFLAGS += -DTLS_DEFAULT_CIPHERS=\"$(CONFIG_TLS_DEFAULT_CIPHERS)\"
endif

ifeq ($(CONFIG_TLS), gnutls)
ifndef CONFIG_CRYPTO
# default to libgcrypt
CONFIG_CRYPTO=gnutls
endif
ifdef TLS_FUNCS
OBJS += src/crypto/tls_gnutls.c
LIBS += -lgnutls -lgpg-error
endif
OBJS += src/crypto/crypto_$(CONFIG_CRYPTO).c
HOBJS += src/crypto/crypto_$(CONFIG_CRYPTO).c
ifdef NEED_FIPS186_2_PRF
OBJS += src/crypto/fips_prf_internal.c
OBJS += src/crypto/sha1-internal.c
endif
ifeq ($(CONFIG_CRYPTO), gnutls)
LIBS += -lgcrypt
LIBS_h += -lgcrypt
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
OBJS += src/crypto/crypto_internal-rsa.c
OBJS += src/crypto/tls_internal.c
OBJS += src/tls/tlsv1_common.c
OBJS += src/tls/tlsv1_record.c
OBJS += src/tls/tlsv1_cred.c
OBJS += src/tls/tlsv1_server.c
OBJS += src/tls/tlsv1_server_write.c
OBJS += src/tls/tlsv1_server_read.c
OBJS += src/tls/asn1.c
OBJS += src/tls/rsa.c
OBJS += src/tls/x509v3.c
OBJS += src/tls/pkcs1.c
OBJS += src/tls/pkcs5.c
OBJS += src/tls/pkcs8.c
NEED_SHA256=y
NEED_BASE64=y
NEED_TLS_PRF=y
ifdef CONFIG_TLSV12
NEED_TLS_PRF_SHA256=y
endif
NEED_MODEXP=y
NEED_CIPHER=y
L_CFLAGS += -DCONFIG_TLS_INTERNAL
L_CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER
endif
ifdef NEED_CIPHER
NEED_DES=y
OBJS += src/crypto/crypto_internal-cipher.c
endif
ifdef NEED_MODEXP
OBJS += src/crypto/crypto_internal-modexp.c
OBJS += src/tls/bignum.c
endif
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
OBJS += src/crypto/crypto_libtomcrypt.c
LIBS += -ltomcrypt -ltfm
LIBS_h += -ltomcrypt -ltfm
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif
ifeq ($(CONFIG_CRYPTO), internal)
OBJS += src/crypto/crypto_internal.c
NEED_AES_DEC=y
L_CFLAGS += -DCONFIG_CRYPTO_INTERNAL
ifdef CONFIG_INTERNAL_LIBTOMMATH
L_CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
ifdef CONFIG_INTERNAL_LIBTOMMATH_FAST
L_CFLAGS += -DLTM_FAST
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
OBJS += src/crypto/crypto_cryptoapi.c
OBJS_p += src/crypto/crypto_cryptoapi.c
L_CFLAGS += -DCONFIG_CRYPTO_CRYPTOAPI
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
endif
endif

ifeq ($(CONFIG_TLS), none)
ifdef TLS_FUNCS
OBJS += src/crypto/tls_none.c
L_CFLAGS += -DEAP_TLS_NONE
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD5=y
endif
OBJS += src/crypto/crypto_none.c
OBJS_p += src/crypto/crypto_none.c
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
endif

ifndef TLS_FUNCS
OBJS += src/crypto/tls_none.c
ifeq ($(CONFIG_TLS), internal)
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_RC4=y
endif
endif

AESOBJS = # none so far
ifdef CONFIG_INTERNAL_AES
AESOBJS += src/crypto/aes-internal.c src/crypto/aes-internal-enc.c
endif

ifneq ($(CONFIG_TLS), openssl)
AESOBJS += src/crypto/aes-wrap.c
endif
ifdef NEED_AES_EAX
AESOBJS += src/crypto/aes-eax.c
NEED_AES_CTR=y
NEED_AES_OMAC1=y
endif
ifdef NEED_AES_SIV
AESOBJS += src/crypto/aes-siv.c
NEED_AES_CTR=y
NEED_AES_OMAC1=y
endif
ifdef NEED_AES_CTR
AESOBJS += src/crypto/aes-ctr.c
endif
ifdef NEED_AES_ENCBLOCK
AESOBJS += src/crypto/aes-encblock.c
endif
ifdef NEED_AES_OMAC1
AESOBJS += src/crypto/aes-omac1.c
endif
ifdef NEED_AES_UNWRAP
ifneq ($(CONFIG_TLS), openssl)
NEED_AES_DEC=y
AESOBJS += src/crypto/aes-unwrap.c
endif
endif
ifdef NEED_AES_CBC
NEED_AES_DEC=y
ifneq ($(CONFIG_TLS), openssl)
AESOBJS += src/crypto/aes-cbc.c
endif
endif
ifdef NEED_AES_DEC
ifdef CONFIG_INTERNAL_AES
AESOBJS += src/crypto/aes-internal-dec.c
endif
endif
ifdef NEED_AES
OBJS += $(AESOBJS)
endif

SHA1OBJS =
ifdef NEED_SHA1
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), gnutls)
SHA1OBJS += src/crypto/sha1.c
endif
endif
SHA1OBJS += src/crypto/sha1-prf.c
ifdef CONFIG_INTERNAL_SHA1
SHA1OBJS += src/crypto/sha1-internal.c
ifdef NEED_FIPS186_2_PRF
SHA1OBJS += src/crypto/fips_prf_internal.c
endif
endif
ifneq ($(CONFIG_TLS), openssl)
SHA1OBJS += src/crypto/sha1-pbkdf2.c
endif
ifdef NEED_T_PRF
SHA1OBJS += src/crypto/sha1-tprf.c
endif
ifdef NEED_TLS_PRF
SHA1OBJS += src/crypto/sha1-tlsprf.c
endif
endif

ifdef NEED_SHA1
OBJS += $(SHA1OBJS)
endif

ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), gnutls)
OBJS += src/crypto/md5.c
endif
endif

ifdef NEED_MD5
ifdef CONFIG_INTERNAL_MD5
OBJS += src/crypto/md5-internal.c
HOBJS += src/crypto/md5-internal.c
endif
endif

ifdef NEED_MD4
ifdef CONFIG_INTERNAL_MD4
OBJS += src/crypto/md4-internal.c
endif
endif

ifdef NEED_DES
ifdef CONFIG_INTERNAL_DES
OBJS += src/crypto/des-internal.c
endif
endif

ifdef CONFIG_NO_RC4
L_CFLAGS += -DCONFIG_NO_RC4
endif

ifdef NEED_RC4
ifdef CONFIG_INTERNAL_RC4
ifndef CONFIG_NO_RC4
OBJS += src/crypto/rc4.c
endif
endif
endif

ifdef NEED_SHA256
L_CFLAGS += -DCONFIG_SHA256
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), gnutls)
OBJS += src/crypto/sha256.c
endif
endif
OBJS += src/crypto/sha256-prf.c
ifdef CONFIG_INTERNAL_SHA256
OBJS += src/crypto/sha256-internal.c
endif
ifdef NEED_TLS_PRF_SHA256
OBJS += src/crypto/sha256-tlsprf.c
endif
ifdef NEED_HMAC_SHA256_KDF
OBJS += src/crypto/sha256-kdf.c
endif
ifdef NEED_HMAC_SHA384_KDF
OBJS += src/crypto/sha384-kdf.c
endif
ifdef NEED_HMAC_SHA512_KDF
OBJS += src/crypto/sha512-kdf.c
endif
endif
ifdef NEED_SHA384
L_CFLAGS += -DCONFIG_SHA384
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), gnutls)
OBJS += src/crypto/sha384.c
endif
endif
OBJS += src/crypto/sha384-prf.c
endif
ifdef NEED_SHA512
L_CFLAGS += -DCONFIG_SHA512
ifneq ($(CONFIG_TLS), openssl)
ifneq ($(CONFIG_TLS), linux)
ifneq ($(CONFIG_TLS), gnutls)
OBJS += src/crypto/sha512.c
endif
endif
endif
OBJS += src/crypto/sha512-prf.c
endif

ifdef CONFIG_INTERNAL_SHA384
L_CFLAGS += -DCONFIG_INTERNAL_SHA384
OBJS += src/crypto/sha384-internal.c
endif

ifdef CONFIG_INTERNAL_SHA512
L_CFLAGS += -DCONFIG_INTERNAL_SHA512
OBJS += src/crypto/sha512-internal.c
endif

ifdef NEED_DH_GROUPS
OBJS += src/crypto/dh_groups.c
endif
ifdef NEED_DH_GROUPS_ALL
L_CFLAGS += -DALL_DH_GROUPS
endif
ifdef CONFIG_INTERNAL_DH_GROUP5
ifdef NEED_DH_GROUPS
OBJS += src/crypto/dh_group5.c
endif
endif

ifdef NEED_ECC
L_CFLAGS += -DCONFIG_ECC
endif

ifdef CONFIG_NO_RANDOM_POOL
L_CFLAGS += -DCONFIG_NO_RANDOM_POOL
else
OBJS += src/crypto/random.c
HOBJS += src/crypto/random.c
HOBJS += src/utils/eloop.c
HOBJS += $(SHA1OBJS)
ifneq ($(CONFIG_TLS), openssl)
HOBJS += src/crypto/md5.c
endif
endif

ifdef CONFIG_RADIUS_SERVER
L_CFLAGS += -DRADIUS_SERVER
OBJS += src/radius/radius_server.c
endif

ifdef CONFIG_IPV6
L_CFLAGS += -DCONFIG_IPV6
endif

ifdef CONFIG_DRIVER_RADIUS_ACL
L_CFLAGS += -DCONFIG_DRIVER_RADIUS_ACL
endif

ifdef NEED_BASE64
OBJS += src/utils/base64.c
endif

ifdef NEED_JSON
OBJS += src/utils/json.c
L_CFLAGS += -DCONFIG_JSON
endif

ifdef NEED_AP_MLME
OBJS += src/ap/wmm.c
OBJS += src/ap/ap_list.c
OBJS += src/ap/ieee802_11.c
OBJS += src/ap/hw_features.c
OBJS += src/ap/dfs.c
L_CFLAGS += -DNEED_AP_MLME
endif
ifdef CONFIG_IEEE80211N
OBJS += src/ap/ieee802_11_ht.c
endif

ifdef CONFIG_IEEE80211AC
OBJS += src/ap/ieee802_11_vht.c
endif

ifdef CONFIG_IEEE80211AX
OBJS += src/ap/ieee802_11_he.c
endif

ifdef CONFIG_P2P_MANAGER
L_CFLAGS += -DCONFIG_P2P_MANAGER
OBJS += src/ap/p2p_hostapd.c
endif

ifdef CONFIG_HS20
L_CFLAGS += -DCONFIG_HS20
OBJS += src/ap/hs20.c
CONFIG_INTERWORKING=y
endif

ifdef CONFIG_INTERWORKING
L_CFLAGS += -DCONFIG_INTERWORKING
NEED_GAS=y
endif

ifdef NEED_GAS
OBJS += src/common/gas.c
OBJS += src/ap/gas_serv.c
endif

ifdef CONFIG_PROXYARP
L_CFLAGS += -DCONFIG_PROXYARP
OBJS += src/ap/x_snoop.c
OBJS += src/ap/dhcp_snoop.c
ifdef CONFIG_IPV6
OBJS += src/ap/ndisc_snoop.c
endif
endif

OBJS += src/drivers/driver_common.c

ifdef CONFIG_ACS
L_CFLAGS += -DCONFIG_ACS
OBJS += src/ap/acs.c
LIBS += -lm
endif

ifdef CONFIG_NO_STDOUT_DEBUG
L_CFLAGS += -DCONFIG_NO_STDOUT_DEBUG
endif

ifdef CONFIG_DEBUG_SYSLOG
L_CFLAGS += -DCONFIG_DEBUG_SYSLOG
endif

ifdef CONFIG_DEBUG_LINUX_TRACING
L_CFLAGS += -DCONFIG_DEBUG_LINUX_TRACING
endif

ifdef CONFIG_DEBUG_FILE
L_CFLAGS += -DCONFIG_DEBUG_FILE
endif

ifdef CONFIG_ANDROID_LOG
L_CFLAGS += -DCONFIG_ANDROID_LOG
endif

OBJS_c = hostapd_cli.c
OBJS_c += src/common/wpa_ctrl.c
OBJS_c += src/utils/os_$(CONFIG_OS).c
OBJS_c += src/common/cli.c
OBJS_c += src/utils/eloop.c
OBJS_c += src/utils/common.c
ifdef CONFIG_WPA_TRACE
OBJS_c += src/utils/trace.c
endif
OBJS_c += src/utils/wpa_debug.c
ifdef CONFIG_WPA_CLI_EDIT
OBJS_c += src/utils/edit.c
else
OBJS_c += src/utils/edit_simple.c
endif

########################

include $(CLEAR_VARS)
LOCAL_MODULE := hostapd_cli
LOCAL_MODULE_TAGS := debug
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := libc libcutils liblog
LOCAL_CFLAGS := $(L_CFLAGS)
LOCAL_SRC_FILES := $(OBJS_c)
LOCAL_C_INCLUDES := $(INCLUDES)
include $(BUILD_EXECUTABLE)

########################
include $(CLEAR_VARS)
LOCAL_MODULE := hostapd
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
ifdef CONFIG_DRIVER_CUSTOM
LOCAL_STATIC_LIBRARIES := libCustomWifi
endif
ifneq ($(BOARD_HOSTAPD_PRIVATE_LIB),)
LOCAL_STATIC_LIBRARIES += $(BOARD_HOSTAPD_PRIVATE_LIB)
endif
LOCAL_SHARED_LIBRARIES := libc libcutils liblog libcrypto libssl
ifdef CONFIG_DRIVER_NL80211
ifneq ($(wildcard external/libnl),)
LOCAL_SHARED_LIBRARIES += libnl
else
LOCAL_STATIC_LIBRARIES += libnl_2
endif
endif
LOCAL_CFLAGS := $(L_CFLAGS)
LOCAL_SRC_FILES := $(OBJS)
LOCAL_C_INCLUDES := $(INCLUDES)
LOCAL_INIT_RC := hostapd.android.rc
include $(BUILD_EXECUTABLE)

endif # ifeq ($(WPA_BUILD_HOSTAPD),true)
