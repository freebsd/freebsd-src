ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

export LIBDIR ?= /usr/local/lib/
export BINDIR ?= /usr/local/sbin/

CFLAGS += -I../src
CFLAGS += -I../src/utils

ALL=wpa_supplicant wpa_passphrase wpa_cli

all: verify_config $(ALL) dynamic_eap_methods

verify_config:
	@if [ ! -r .config ]; then \
		echo 'Building wpa_supplicant requires a configuration file'; \
		echo '(.config). See README for more instructions. You can'; \
		echo 'run "cp defconfig .config" to create an example'; \
		echo 'configuration.'; \
		exit 1; \
	fi

mkconfig:
	@if [ -f .config ]; then \
		echo '.config exists - did not replace it'; \
		exit 1; \
	fi
	echo CONFIG_DRIVER_HOSTAP=y >> .config
	echo CONFIG_DRIVER_WEXT=y >> .config

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	for i in $(ALL); do cp $$i $(DESTDIR)$(BINDIR)/$$i; done
	$(MAKE) -C ../src install

OBJS = config.o
OBJS += notify.o
OBJS += bss.o
OBJS += eap_register.o
OBJS += ../src/utils/common.o
OBJS += ../src/utils/wpa_debug.o
OBJS += ../src/utils/wpabuf.o
OBJS_p = wpa_passphrase.o
OBJS_p += ../src/utils/common.o
OBJS_p += ../src/utils/wpa_debug.o
OBJS_p += ../src/utils/wpabuf.o
OBJS_c = wpa_cli.o ../src/common/wpa_ctrl.o

-include .config

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

OBJS += ../src/utils/os_$(CONFIG_OS).o
OBJS_p += ../src/utils/os_$(CONFIG_OS).o
OBJS_c += ../src/utils/os_$(CONFIG_OS).o

ifdef CONFIG_WPA_TRACE
CFLAGS += -DWPA_TRACE
OBJS += ../src/utils/trace.o
OBJS_p += ../src/utils/trace.o
OBJS_c += ../src/utils/trace.o
OBJS_c += ../src/utils/wpa_debug.o
LDFLAGS += -rdynamic
CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
CFLAGS += -DWPA_TRACE_BFD
LIBS += -lbfd
LIBS_p += -lbfd
LIBS_c += -lbfd
endif
endif

ifndef CONFIG_ELOOP
CONFIG_ELOOP=eloop
endif
OBJS += ../src/utils/$(CONFIG_ELOOP).o


ifdef CONFIG_EAPOL_TEST
CFLAGS += -Werror -DEAPOL_TEST
endif

ifndef CONFIG_BACKEND
CONFIG_BACKEND=file
endif

ifeq ($(CONFIG_BACKEND), file)
OBJS += config_file.o
ifndef CONFIG_NO_CONFIG_BLOBS
NEED_BASE64=y
endif
CFLAGS += -DCONFIG_BACKEND_FILE
endif

ifeq ($(CONFIG_BACKEND), winreg)
OBJS += config_winreg.o
endif

ifeq ($(CONFIG_BACKEND), none)
OBJS += config_none.o
endif

ifdef CONFIG_NO_CONFIG_WRITE
CFLAGS += -DCONFIG_NO_CONFIG_WRITE
endif

ifdef CONFIG_NO_CONFIG_BLOBS
CFLAGS += -DCONFIG_NO_CONFIG_BLOBS
endif

ifdef CONFIG_NO_SCAN_PROCESSING
CFLAGS += -DCONFIG_NO_SCAN_PROCESSING
endif

ifdef CONFIG_IEEE80211W
CFLAGS += -DCONFIG_IEEE80211W
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_IEEE80211R
CFLAGS += -DCONFIG_IEEE80211R
OBJS += ../src/rsn_supp/wpa_ft.o
NEED_80211_COMMON=y
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_PEERKEY
CFLAGS += -DCONFIG_PEERKEY
endif

ifndef CONFIG_NO_WPA
OBJS += ../src/rsn_supp/wpa.o
OBJS += ../src/rsn_supp/preauth.o
OBJS += ../src/rsn_supp/pmksa_cache.o
OBJS += ../src/rsn_supp/peerkey.o
OBJS += ../src/rsn_supp/wpa_ie.o
OBJS += ../src/common/wpa_common.o
NEED_AES=y
NEED_SHA1=y
NEED_MD5=y
NEED_RC4=y
else
CFLAGS += -DCONFIG_NO_WPA -DCONFIG_NO_WPA2
endif

ifdef CONFIG_IBSS_RSN
NEED_RSN_AUTHENTICATOR=y
CFLAGS += -DCONFIG_IBSS_RSN
OBJS += ibss_rsn.o
endif

ifdef CONFIG_NO_WPA2
CFLAGS += -DCONFIG_NO_WPA2
endif

include ../src/drivers/drivers.mak
ifdef CONFIG_AP
OBJS_d += $(DRV_BOTH_OBJS)
CFLAGS += $(DRV_BOTH_CFLAGS)
LDFLAGS += $(DRV_BOTH_LDFLAGS)
LIBS += $(DRV_BOTH_LIBS)
else
NEED_AP_MLME=
OBJS_d += $(DRV_WPA_OBJS)
CFLAGS += $(DRV_WPA_CFLAGS)
LDFLAGS += $(DRV_WPA_LDFLAGS)
LIBS += $(DRV_WPA_LIBS)
endif

ifndef CONFIG_L2_PACKET
CONFIG_L2_PACKET=linux
endif

OBJS_l2 += ../src/l2_packet/l2_packet_$(CONFIG_L2_PACKET).o

ifeq ($(CONFIG_L2_PACKET), pcap)
ifdef CONFIG_WINPCAP
CFLAGS += -DCONFIG_WINPCAP
LIBS += -lwpcap -lpacket
LIBS_w += -lwpcap
else
LIBS += -ldnet -lpcap
endif
endif

ifeq ($(CONFIG_L2_PACKET), winpcap)
LIBS += -lwpcap -lpacket
LIBS_w += -lwpcap
endif

ifeq ($(CONFIG_L2_PACKET), freebsd)
LIBS += -lpcap
endif

ifdef CONFIG_EAP_TLS
# EAP-TLS
ifeq ($(CONFIG_EAP_TLS), dyn)
CFLAGS += -DEAP_TLS_DYNAMIC
EAPDYN += ../src/eap_peer/eap_tls.so
else
CFLAGS += -DEAP_TLS
OBJS += ../src/eap_peer/eap_tls.o
OBJS_h += ../src/eap_server/eap_server_tls.o
endif
TLS_FUNCS=y
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_PEAP
# EAP-PEAP
ifeq ($(CONFIG_EAP_PEAP), dyn)
CFLAGS += -DEAP_PEAP_DYNAMIC
EAPDYN += ../src/eap_peer/eap_peap.so
else
CFLAGS += -DEAP_PEAP
OBJS += ../src/eap_peer/eap_peap.o
OBJS += ../src/eap_common/eap_peap_common.o
OBJS_h += ../src/eap_server/eap_server_peap.o
endif
TLS_FUNCS=y
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_TTLS
# EAP-TTLS
ifeq ($(CONFIG_EAP_TTLS), dyn)
CFLAGS += -DEAP_TTLS_DYNAMIC
EAPDYN += ../src/eap_peer/eap_ttls.so
else
CFLAGS += -DEAP_TTLS
OBJS += ../src/eap_peer/eap_ttls.o
OBJS_h += ../src/eap_server/eap_server_ttls.o
endif
MS_FUNCS=y
TLS_FUNCS=y
CHAP=y
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_MD5
# EAP-MD5
ifeq ($(CONFIG_EAP_MD5), dyn)
CFLAGS += -DEAP_MD5_DYNAMIC
EAPDYN += ../src/eap_peer/eap_md5.so
else
CFLAGS += -DEAP_MD5
OBJS += ../src/eap_peer/eap_md5.o
OBJS_h += ../src/eap_server/eap_server_md5.o
endif
CHAP=y
CONFIG_IEEE8021X_EAPOL=y
endif

# backwards compatibility for old spelling
ifdef CONFIG_MSCHAPV2
ifndef CONFIG_EAP_MSCHAPV2
CONFIG_EAP_MSCHAPV2=y
endif
endif

ifdef CONFIG_EAP_MSCHAPV2
# EAP-MSCHAPv2
ifeq ($(CONFIG_EAP_MSCHAPV2), dyn)
CFLAGS += -DEAP_MSCHAPv2_DYNAMIC
EAPDYN += ../src/eap_peer/eap_mschapv2.so
EAPDYN += ../src/eap_peer/mschapv2.so
else
CFLAGS += -DEAP_MSCHAPv2
OBJS += ../src/eap_peer/eap_mschapv2.o
OBJS += ../src/eap_peer/mschapv2.o
OBJS_h += ../src/eap_server/eap_server_mschapv2.o
endif
MS_FUNCS=y
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_GTC
# EAP-GTC
ifeq ($(CONFIG_EAP_GTC), dyn)
CFLAGS += -DEAP_GTC_DYNAMIC
EAPDYN += ../src/eap_peer/eap_gtc.so
else
CFLAGS += -DEAP_GTC
OBJS += ../src/eap_peer/eap_gtc.o
OBJS_h += ../src/eap_server/eap_server_gtc.o
endif
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_OTP
# EAP-OTP
ifeq ($(CONFIG_EAP_OTP), dyn)
CFLAGS += -DEAP_OTP_DYNAMIC
EAPDYN += ../src/eap_peer/eap_otp.so
else
CFLAGS += -DEAP_OTP
OBJS += ../src/eap_peer/eap_otp.o
endif
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_SIM
# EAP-SIM
ifeq ($(CONFIG_EAP_SIM), dyn)
CFLAGS += -DEAP_SIM_DYNAMIC
EAPDYN += ../src/eap_peer/eap_sim.so
else
CFLAGS += -DEAP_SIM
OBJS += ../src/eap_peer/eap_sim.o
OBJS_h += ../src/eap_server/eap_server_sim.o
endif
CONFIG_IEEE8021X_EAPOL=y
CONFIG_EAP_SIM_COMMON=y
NEED_AES_CBC=y
endif

ifdef CONFIG_EAP_LEAP
# EAP-LEAP
ifeq ($(CONFIG_EAP_LEAP), dyn)
CFLAGS += -DEAP_LEAP_DYNAMIC
EAPDYN += ../src/eap_peer/eap_leap.so
else
CFLAGS += -DEAP_LEAP
OBJS += ../src/eap_peer/eap_leap.o
endif
MS_FUNCS=y
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_PSK
# EAP-PSK
ifeq ($(CONFIG_EAP_PSK), dyn)
CFLAGS += -DEAP_PSK_DYNAMIC
EAPDYN += ../src/eap_peer/eap_psk.so
else
CFLAGS += -DEAP_PSK
OBJS += ../src/eap_peer/eap_psk.o ../src/eap_common/eap_psk_common.o
OBJS_h += ../src/eap_server/eap_server_psk.o
endif
CONFIG_IEEE8021X_EAPOL=y
NEED_AES=y
NEED_AES_OMAC1=y
NEED_AES_ENCBLOCK=y
NEED_AES_EAX=y
endif

ifdef CONFIG_EAP_AKA
# EAP-AKA
ifeq ($(CONFIG_EAP_AKA), dyn)
CFLAGS += -DEAP_AKA_DYNAMIC
EAPDYN += ../src/eap_peer/eap_aka.so
else
CFLAGS += -DEAP_AKA
OBJS += ../src/eap_peer/eap_aka.o
OBJS_h += ../src/eap_server/eap_server_aka.o
endif
CONFIG_IEEE8021X_EAPOL=y
CONFIG_EAP_SIM_COMMON=y
NEED_AES_CBC=y
endif

ifdef CONFIG_EAP_AKA_PRIME
# EAP-AKA'
ifeq ($(CONFIG_EAP_AKA_PRIME), dyn)
CFLAGS += -DEAP_AKA_PRIME_DYNAMIC
else
CFLAGS += -DEAP_AKA_PRIME
endif
NEED_SHA256=y
endif

ifdef CONFIG_EAP_SIM_COMMON
OBJS += ../src/eap_common/eap_sim_common.o
OBJS_h += ../src/eap_server/eap_sim_db.o
NEED_AES=y
NEED_FIPS186_2_PRF=y
endif

ifdef CONFIG_EAP_FAST
# EAP-FAST
ifeq ($(CONFIG_EAP_FAST), dyn)
CFLAGS += -DEAP_FAST_DYNAMIC
EAPDYN += ../src/eap_peer/eap_fast.so
EAPDYN += ../src/eap_common/eap_fast_common.o
else
CFLAGS += -DEAP_FAST
OBJS += ../src/eap_peer/eap_fast.o ../src/eap_peer/eap_fast_pac.o
OBJS += ../src/eap_common/eap_fast_common.o
OBJS_h += ../src/eap_server/eap_server_fast.o
endif
TLS_FUNCS=y
CONFIG_IEEE8021X_EAPOL=y
NEED_T_PRF=y
endif

ifdef CONFIG_EAP_PAX
# EAP-PAX
ifeq ($(CONFIG_EAP_PAX), dyn)
CFLAGS += -DEAP_PAX_DYNAMIC
EAPDYN += ../src/eap_peer/eap_pax.so
else
CFLAGS += -DEAP_PAX
OBJS += ../src/eap_peer/eap_pax.o ../src/eap_common/eap_pax_common.o
OBJS_h += ../src/eap_server/eap_server_pax.o
endif
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_SAKE
# EAP-SAKE
ifeq ($(CONFIG_EAP_SAKE), dyn)
CFLAGS += -DEAP_SAKE_DYNAMIC
EAPDYN += ../src/eap_peer/eap_sake.so
else
CFLAGS += -DEAP_SAKE
OBJS += ../src/eap_peer/eap_sake.o ../src/eap_common/eap_sake_common.o
OBJS_h += ../src/eap_server/eap_server_sake.o
endif
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_GPSK
# EAP-GPSK
ifeq ($(CONFIG_EAP_GPSK), dyn)
CFLAGS += -DEAP_GPSK_DYNAMIC
EAPDYN += ../src/eap_peer/eap_gpsk.so
else
CFLAGS += -DEAP_GPSK
OBJS += ../src/eap_peer/eap_gpsk.o ../src/eap_common/eap_gpsk_common.o
OBJS_h += ../src/eap_server/eap_server_gpsk.o
endif
CONFIG_IEEE8021X_EAPOL=y
ifdef CONFIG_EAP_GPSK_SHA256
CFLAGS += -DEAP_GPSK_SHA256
endif
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_WPS
# EAP-WSC
CFLAGS += -DCONFIG_WPS -DEAP_WSC
OBJS += wps_supplicant.o
OBJS += ../src/utils/uuid.o
OBJS += ../src/eap_peer/eap_wsc.o ../src/eap_common/eap_wsc_common.o
OBJS += ../src/wps/wps.o
OBJS += ../src/wps/wps_common.o
OBJS += ../src/wps/wps_attr_parse.o
OBJS += ../src/wps/wps_attr_build.o
OBJS += ../src/wps/wps_attr_process.o
OBJS += ../src/wps/wps_dev_attr.o
OBJS += ../src/wps/wps_enrollee.o
OBJS += ../src/wps/wps_registrar.o
OBJS_h += ../src/eap_server/eap_server_wsc.o
CONFIG_IEEE8021X_EAPOL=y
NEED_DH_GROUPS=y
NEED_SHA256=y
NEED_BASE64=y
NEED_80211_COMMON=y
NEED_AES_CBC=y
NEED_MODEXP=y

ifdef CONFIG_WPS_UFD
CFLAGS += -DCONFIG_WPS_UFD
OBJS += ../src/wps/wps_ufd.o
NEED_WPS_OOB=y
endif

ifdef CONFIG_WPS_NFC
CFLAGS += -DCONFIG_WPS_NFC
OBJS += ../src/wps/ndef.o
OBJS += ../src/wps/wps_nfc.o
NEED_WPS_OOB=y
ifdef CONFIG_WPS_NFC_PN531
PN531_PATH ?= /usr/local/src/nfc
CFLAGS += -DCONFIG_WPS_NFC_PN531
CFLAGS += -I${PN531_PATH}/inc
OBJS += ../src/wps/wps_nfc_pn531.o
LIBS += ${PN531_PATH}/lib/wpsnfc.dll
LIBS += ${PN531_PATH}/lib/libnfc_mapping_pn53x.dll
endif
endif

ifdef NEED_WPS_OOB
CFLAGS += -DCONFIG_WPS_OOB
endif

ifdef CONFIG_WPS_ER
CONFIG_WPS_UPNP=y
CFLAGS += -DCONFIG_WPS_ER
OBJS += ../src/wps/wps_er.o
OBJS += ../src/wps/wps_er_ssdp.o
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

endif

ifdef CONFIG_EAP_IKEV2
# EAP-IKEv2
ifeq ($(CONFIG_EAP_IKEV2), dyn)
CFLAGS += -DEAP_IKEV2_DYNAMIC
EAPDYN += ../src/eap_peer/eap_ikev2.so ../src/eap_peer/ikev2.o
EAPDYN += ../src/eap_common/eap_ikev2_common.o ../src/eap_common/ikev2_common.o
else
CFLAGS += -DEAP_IKEV2
OBJS += ../src/eap_peer/eap_ikev2.o ../src/eap_peer/ikev2.o
OBJS += ../src/eap_common/eap_ikev2_common.o ../src/eap_common/ikev2_common.o
OBJS_h += ../src/eap_server/eap_server_ikev2.o
OBJS_h += ../src/eap_server/ikev2.o
endif
CONFIG_IEEE8021X_EAPOL=y
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
NEED_MODEXP=y
NEED_CIPHER=y
endif

ifdef CONFIG_EAP_VENDOR_TEST
ifeq ($(CONFIG_EAP_VENDOR_TEST), dyn)
CFLAGS += -DEAP_VENDOR_TEST_DYNAMIC
EAPDYN += ../src/eap_peer/eap_vendor_test.so
else
CFLAGS += -DEAP_VENDOR_TEST
OBJS += ../src/eap_peer/eap_vendor_test.o
OBJS_h += ../src/eap_server/eap_server_vendor_test.o
endif
CONFIG_IEEE8021X_EAPOL=y
endif

ifdef CONFIG_EAP_TNC
# EAP-TNC
CFLAGS += -DEAP_TNC
OBJS += ../src/eap_peer/eap_tnc.o
OBJS += ../src/eap_peer/tncc.o
OBJS_h += ../src/eap_server/eap_server_tnc.o
OBJS_h += ../src/eap_server/tncs.o
NEED_BASE64=y
ifndef CONFIG_NATIVE_WINDOWS
ifndef CONFIG_DRIVER_BSD
LIBS += -ldl
endif
endif
endif

ifdef CONFIG_IEEE8021X_EAPOL
# IEEE 802.1X/EAPOL state machines (e.g., for RADIUS authentication)
CFLAGS += -DIEEE8021X_EAPOL
OBJS += ../src/eapol_supp/eapol_supp_sm.o
OBJS += ../src/eap_peer/eap.o ../src/eap_peer/eap_methods.o
NEED_EAP_COMMON=y
ifdef CONFIG_DYNAMIC_EAP_METHODS
CFLAGS += -DCONFIG_DYNAMIC_EAP_METHODS
LIBS += -ldl -rdynamic
endif
endif

ifdef CONFIG_AP
NEED_80211_COMMON=y
NEED_EAP_COMMON=y
NEED_RSN_AUTHENTICATOR=y
CFLAGS += -DCONFIG_AP
OBJS += ap.o
CFLAGS += -DCONFIG_NO_RADIUS
CFLAGS += -DCONFIG_NO_ACCOUNTING
CFLAGS += -DCONFIG_NO_VLAN
OBJS += ../src/ap/hostapd.o
OBJS += ../src/ap/wpa_auth_glue.o
OBJS += ../src/ap/utils.o
OBJS += ../src/ap/authsrv.o
OBJS += ../src/ap/ap_config.o
OBJS += ../src/utils/ip_addr.o
OBJS += ../src/ap/sta_info.o
OBJS += ../src/ap/tkip_countermeasures.o
OBJS += ../src/ap/ap_mlme.o
OBJS += ../src/ap/ieee802_1x.o
OBJS += ../src/eapol_auth/eapol_auth_sm.o
OBJS += ../src/ap/ieee802_11_auth.o
OBJS += ../src/ap/drv_callbacks.o
OBJS += ../src/ap/ap_drv_ops.o
ifdef CONFIG_CTRL_IFACE
OBJS += ../src/ap/ctrl_iface_ap.o
endif

CFLAGS += -DEAP_SERVER -DEAP_SERVER_IDENTITY
OBJS += ../src/eap_server/eap_server.o
OBJS += ../src/eap_server/eap_server_identity.o
OBJS += ../src/eap_server/eap_server_methods.o

ifdef CONFIG_IEEE80211N
CFLAGS += -DCONFIG_IEEE80211N
endif

ifdef NEED_AP_MLME
OBJS += ../src/ap/beacon.o
OBJS += ../src/ap/wmm.o
OBJS += ../src/ap/ap_list.o
OBJS += ../src/ap/ieee802_11.o
OBJS += ../src/ap/hw_features.o
ifdef CONFIG_IEEE80211N
OBJS += ../src/ap/ieee802_11_ht.o
endif
CFLAGS += -DNEED_AP_MLME
endif
ifdef CONFIG_WPS
CFLAGS += -DEAP_SERVER_WSC
OBJS += ../src/ap/wps_hostapd.o
OBJS += ../src/eap_server/eap_server_wsc.o
endif
endif

ifdef NEED_RSN_AUTHENTICATOR
CFLAGS += -DCONFIG_NO_RADIUS
NEED_AES_WRAP=y
OBJS += ../src/ap/wpa_auth.o
OBJS += ../src/ap/wpa_auth_ie.o
OBJS += ../src/ap/pmksa_cache_auth.o
ifdef CONFIG_IEEE80211R
OBJS += ../src/ap/wpa_auth_ft.o
endif
ifdef CONFIG_PEERKEY
OBJS += ../src/ap/peerkey_auth.o
endif
endif

ifdef CONFIG_EAP_SERVER
CFLAGS += -DEAP_SERVER
OBJS_h += ../src/eap_server/eap_server.o
OBJS_h += ../src/eap_server/eap_server_identity.o
OBJS_h += ../src/eap_server/eap_server_methods.o
endif

ifdef CONFIG_RADIUS_CLIENT
OBJS_h += ../src/utils/ip_addr.o
OBJS_h += ../src/radius/radius.o
OBJS_h += ../src/radius/radius_client.o
endif

ifdef CONFIG_AUTHENTICATOR
OBJS_h += ../src/eapol_auth/eapol_auth_sm.o
OBJS_h += ../src/ap/ieee802_1x.o
endif

ifdef CONFIG_WPA_AUTHENTICATOR
OBJS_h += ../src/ap/wpa_auth.o
OBJS_h += ../src/ap/wpa_auth_ie.o
OBJS_h += ../src/ap/pmksa_cache_auth.o
ifdef CONFIG_IEEE80211R
OBJS_h += ../src/ap/wpa_auth_ft.o
endif
ifdef CONFIG_PEERKEY
OBJS_h += ../src/ap/peerkey_auth.o
endif
endif

ifdef CONFIG_PCSC
# PC/SC interface for smartcards (USIM, GSM SIM)
CFLAGS += -DPCSC_FUNCS -I/usr/include/PCSC
OBJS += ../src/utils/pcsc_funcs.o
# -lpthread may not be needed depending on how pcsc-lite was configured
ifdef CONFIG_NATIVE_WINDOWS
#Once MinGW gets support for WinScard, -lwinscard could be used instead of the
#dynamic symbol loading that is now used in pcsc_funcs.c
#LIBS += -lwinscard
else
LIBS += -lpcsclite -lpthread
endif
endif

ifdef CONFIG_SIM_SIMULATOR
CFLAGS += -DCONFIG_SIM_SIMULATOR
NEED_MILENAGE=y
endif

ifdef CONFIG_USIM_SIMULATOR
CFLAGS += -DCONFIG_USIM_SIMULATOR
NEED_MILENAGE=y
endif

ifdef NEED_MILENAGE
OBJS += ../src/crypto/milenage.o
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef CONFIG_SMARTCARD
CFLAGS += -DCONFIG_SMARTCARD
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
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, EAP_TTLS, and EAP_FAST)
OBJS += ../src/eap_peer/eap_tls_common.o
OBJS_h += ../src/eap_server/eap_server_tls_common.o
NEED_TLS_PRF=y
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifeq ($(CONFIG_TLS), openssl)
ifdef TLS_FUNCS
CFLAGS += -DEAP_TLS_OPENSSL
OBJS += ../src/crypto/tls_openssl.o
LIBS += -lssl
endif
OBJS += ../src/crypto/crypto_openssl.o
OBJS_p += ../src/crypto/crypto_openssl.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_openssl.o
endif
LIBS += -lcrypto
LIBS_p += -lcrypto
endif

ifeq ($(CONFIG_TLS), gnutls)
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_gnutls.o
LIBS += -lgnutls -lgpg-error
ifdef CONFIG_GNUTLS_EXTRA
CFLAGS += -DCONFIG_GNUTLS_EXTRA
LIBS += -lgnutls-extra
endif
endif
OBJS += ../src/crypto/crypto_gnutls.o
OBJS_p += ../src/crypto/crypto_gnutls.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_gnutls.o
endif
LIBS += -lgcrypt
LIBS_p += -lgcrypt
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif

ifeq ($(CONFIG_TLS), schannel)
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_schannel.o
endif
OBJS += ../src/crypto/crypto_cryptoapi.o
OBJS_p += ../src/crypto/crypto_cryptoapi.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_cryptoapi.o
endif
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif

ifeq ($(CONFIG_TLS), nss)
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_nss.o
LIBS += -lssl3
endif
OBJS += ../src/crypto/crypto_nss.o
OBJS_p += ../src/crypto/crypto_nss.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_nss.o
endif
LIBS += -lnss3
LIBS_p += -lnss3
CONFIG_INTERNAL_MD4=y
CONFIG_INTERNAL_DH_GROUP5=y
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
OBJS += ../src/tls/tlsv1_client.o
OBJS += ../src/tls/tlsv1_client_write.o
OBJS += ../src/tls/tlsv1_client_read.o
OBJS += ../src/tls/asn1.o
OBJS += ../src/tls/rsa.o
OBJS += ../src/tls/x509v3.o
OBJS += ../src/tls/pkcs1.o
OBJS += ../src/tls/pkcs5.o
OBJS += ../src/tls/pkcs8.o
NEED_SHA256=y
NEED_BASE64=y
NEED_TLS_PRF=y
NEED_MODEXP=y
NEED_CIPHER=y
CFLAGS += -DCONFIG_TLS_INTERNAL_CLIENT
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
OBJS_p += ../src/crypto/crypto_libtomcrypt.o
LIBS += -ltomcrypt -ltfm
LIBS_p += -ltomcrypt -ltfm
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
CONFIG_INTERNAL_DH_GROUP5=y
endif
ifeq ($(CONFIG_CRYPTO), internal)
OBJS += ../src/crypto/crypto_internal.o
OBJS_p += ../src/crypto/crypto_internal.o
NEED_AES_ENC=y
CFLAGS += -DCONFIG_CRYPTO_INTERNAL
ifdef CONFIG_INTERNAL_LIBTOMMATH
CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
ifdef CONFIG_INTERNAL_LIBTOMMATH_FAST
CFLAGS += -DLTM_FAST
endif
else
LIBS += -ltommath
LIBS_p += -ltommath
endif
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_DES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD4=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_SHA256=y
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

ifdef TLS_FUNCS
ifdef CONFIG_SMARTCARD
ifndef CONFIG_NATIVE_WINDOWS
ifneq ($(CONFIG_L2_PACKET), freebsd)
LIBS += -ldl
endif
endif
endif
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

AESOBJS = # none so far (see below)
ifdef CONFIG_INTERNAL_AES
AESOBJS += ../src/crypto/aes-internal.o ../src/crypto/aes-internal-dec.o
endif

AESOBJS += ../src/crypto/aes-unwrap.o
ifdef NEED_AES_EAX
AESOBJS += ../src/crypto/aes-eax.o
NEED_AES_CTR=y
endif
ifdef NEED_AES_CTR
AESOBJS += ../src/crypto/aes-ctr.o
endif
ifdef NEED_AES_ENCBLOCK
AESOBJS += ../src/crypto/aes-encblock.o
endif
ifdef NEED_AES_OMAC1
NEED_AES_ENC=y
AESOBJS += ../src/crypto/aes-omac1.o
endif
ifdef NEED_AES_WRAP
NEED_AES_ENC=y
AESOBJS += ../src/crypto/aes-wrap.o
endif
ifdef NEED_AES_CBC
NEED_AES_ENC=y
AESOBJS += ../src/crypto/aes-cbc.o
endif
ifdef NEED_AES_ENC
ifdef CONFIG_INTERNAL_AES
AESOBJS += ../src/crypto/aes-internal-enc.o
endif
endif
ifdef NEED_AES
OBJS += $(AESOBJS)
endif

ifdef NEED_SHA1
SHA1OBJS += ../src/crypto/sha1.o
ifdef CONFIG_INTERNAL_SHA1
SHA1OBJS += ../src/crypto/sha1-internal.o
ifdef NEED_FIPS186_2_PRF
SHA1OBJS += ../src/crypto/fips_prf_internal.o
endif
endif
ifndef CONFIG_NO_WPA_PASSPHRASE
SHA1OBJS += ../src/crypto/sha1-pbkdf2.o
endif
ifdef NEED_T_PRF
SHA1OBJS += ../src/crypto/sha1-tprf.o
endif
ifdef NEED_TLS_PRF
SHA1OBJS += ../src/crypto/sha1-tlsprf.o
endif
endif

MD5OBJS = ../src/crypto/md5.o
ifdef NEED_MD5
ifdef CONFIG_INTERNAL_MD5
MD5OBJS += ../src/crypto/md5-internal.o
endif
ifdef CONFIG_FIPS
MD5OBJS += ../src/crypto/md5-non-fips.o
endif
OBJS += $(MD5OBJS)
OBJS_p += $(MD5OBJS)
endif

ifdef NEED_MD4
ifdef CONFIG_INTERNAL_MD4
OBJS += ../src/crypto/md4-internal.o
endif
endif

DESOBJS = # none needed when not internal
ifdef NEED_DES
ifdef CONFIG_INTERNAL_DES
DESOBJS += ../src/crypto/des-internal.o
endif
endif

ifdef NEED_RC4
ifdef CONFIG_INTERNAL_RC4
OBJS += ../src/crypto/rc4.o
endif
endif

SHA256OBJS = # none by default
ifdef NEED_SHA256
CFLAGS += -DCONFIG_SHA256
SHA256OBJS += ../src/crypto/sha256.o
ifdef CONFIG_INTERNAL_SHA256
SHA256OBJS += ../src/crypto/sha256-internal.o
endif
OBJS += $(SHA256OBJS)
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

ifdef CONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), y)
ifdef CONFIG_NATIVE_WINDOWS
CONFIG_CTRL_IFACE=named_pipe
else
CONFIG_CTRL_IFACE=unix
endif
endif
CFLAGS += -DCONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), unix)
CFLAGS += -DCONFIG_CTRL_IFACE_UNIX
endif
ifeq ($(CONFIG_CTRL_IFACE), udp)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
endif
ifeq ($(CONFIG_CTRL_IFACE), named_pipe)
CFLAGS += -DCONFIG_CTRL_IFACE_NAMED_PIPE
endif
OBJS += ctrl_iface.o ctrl_iface_$(CONFIG_CTRL_IFACE).o
endif

ifdef CONFIG_CTRL_IFACE_DBUS
DBUS=y
DBUS_CFLAGS += -DCONFIG_CTRL_IFACE_DBUS -DDBUS_API_SUBJECT_TO_CHANGE
DBUS_OBJS += dbus/dbus_old.o dbus/dbus_old_handlers.o
ifdef CONFIG_WPS
DBUS_OBJS += dbus/dbus_old_handlers_wps.o
endif
DBUS_OBJS += dbus/dbus_dict_helpers.o
ifndef DBUS_LIBS
DBUS_LIBS := $(shell pkg-config --libs dbus-1)
endif
ifndef DBUS_INCLUDE
DBUS_INCLUDE := $(shell pkg-config --cflags dbus-1)
endif
dbus_version=$(subst ., ,$(shell pkg-config --modversion dbus-1))
DBUS_VERSION_MAJOR=$(word 1,$(dbus_version))
DBUS_VERSION_MINOR=$(word 2,$(dbus_version))
ifeq ($(DBUS_VERSION_MAJOR),)
DBUS_VERSION_MAJOR=0
endif
ifeq ($(DBUS_VERSION_MINOR),)
DBUS_VERSION_MINOR=0
endif
DBUS_INCLUDE += -DDBUS_VERSION_MAJOR=$(DBUS_VERSION_MAJOR)
DBUS_INCLUDE += -DDBUS_VERSION_MINOR=$(DBUS_VERSION_MINOR)
DBUS_CFLAGS += $(DBUS_INCLUDE)
endif

ifdef CONFIG_CTRL_IFACE_DBUS_NEW
DBUS=y
DBUS_CFLAGS += -DCONFIG_CTRL_IFACE_DBUS_NEW
DBUS_OBJS ?= dbus/dbus_dict_helpers.o
DBUS_OBJS += dbus/dbus_new_helpers.o
DBUS_OBJS += dbus/dbus_new.o dbus/dbus_new_handlers.o
ifdef CONFIG_WPS
DBUS_OBJS += dbus/dbus_new_handlers_wps.o
endif
ifndef DBUS_LIBS
DBUS_LIBS := $(shell pkg-config --libs dbus-1)
endif
ifndef DBUS_INCLUDE
DBUS_INCLUDE := $(shell pkg-config --cflags dbus-1)
endif
ifdef CONFIG_CTRL_IFACE_DBUS_INTRO
DBUS_OBJS += dbus/dbus_new_introspect.o
DBUS_CFLAGS += -DCONFIG_CTRL_IFACE_DBUS_INTRO
endif
DBUS_CFLAGS += $(DBUS_INCLUDE)
endif

ifdef DBUS
DBUS_CFLAGS += -DCONFIG_DBUS
DBUS_OBJS += dbus/dbus_common.o
endif

OBJS += $(DBUS_OBJS)
CFLAGS += $(DBUS_CFLAGS)
LIBS += $(DBUS_LIBS)

ifdef CONFIG_READLINE
CFLAGS += -DCONFIG_READLINE
LIBS_c += -lncurses -lreadline
endif

ifdef CONFIG_NATIVE_WINDOWS
CFLAGS += -DCONFIG_NATIVE_WINDOWS
LIBS += -lws2_32 -lgdi32 -lcrypt32
LIBS_c += -lws2_32
LIBS_p += -lws2_32 -lgdi32
ifeq ($(CONFIG_CRYPTO), cryptoapi)
LIBS_p += -lcrypt32
endif
endif

ifdef CONFIG_NO_STDOUT_DEBUG
CFLAGS += -DCONFIG_NO_STDOUT_DEBUG
ifndef CONFIG_CTRL_IFACE
CFLAGS += -DCONFIG_NO_WPA_MSG
endif
endif

ifdef CONFIG_IPV6
# for eapol_test only
CFLAGS += -DCONFIG_IPV6
endif

ifdef NEED_BASE64
OBJS += ../src/utils/base64.o
endif

ifdef NEED_SME
NEED_80211_COMMON=y
OBJS += sme.o
CFLAGS += -DCONFIG_SME
endif

ifdef CONFIG_CLIENT_MLME
OBJS += mlme.o
CFLAGS += -DCONFIG_CLIENT_MLME
NEED_80211_COMMON=y
endif

ifdef NEED_80211_COMMON
OBJS += ../src/common/ieee802_11_common.o
endif

ifdef NEED_EAP_COMMON
OBJS += ../src/eap_common/eap_common.o
endif

ifndef CONFIG_MAIN
CONFIG_MAIN=main
endif

ifdef CONFIG_DEBUG_SYSLOG
CFLAGS += -DCONFIG_DEBUG_SYSLOG
endif

ifdef CONFIG_DEBUG_FILE
CFLAGS += -DCONFIG_DEBUG_FILE
endif

ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
CFLAGS += -DCONFIG_DELAYED_MIC_ERROR_REPORT
endif

ifdef CONFIG_FIPS
CFLAGS += -DCONFIG_FIPS
endif

OBJS += $(SHA1OBJS) $(DESOBJS)

OBJS_p += $(SHA1OBJS)

ifdef CONFIG_BGSCAN_SIMPLE
CFLAGS += -DCONFIG_BGSCAN_SIMPLE
OBJS += bgscan_simple.o
NEED_BGSCAN=y
endif

ifdef NEED_BGSCAN
CFLAGS += -DCONFIG_BGSCAN
OBJS += bgscan.o
endif

OBJS_wpa_rm := ctrl_iface.o mlme.o ctrl_iface_unix.o
OBJS_wpa := $(filter-out $(OBJS_wpa_rm),$(OBJS)) $(OBJS_h) tests/test_wpa.o
ifdef CONFIG_AUTHENTICATOR
OBJS_wpa += tests/link_test.o
endif
OBJS_wpa += $(OBJS_l2)
OBJS += wpa_supplicant.o events.o blacklist.o wpas_glue.o scan.o
OBJS_t := $(OBJS) $(OBJS_l2) eapol_test.o
OBJS_t += ../src/radius/radius_client.o
OBJS_t += ../src/radius/radius.o
ifndef CONFIG_AP
OBJS_t += ../src/utils/ip_addr.o
endif
OBJS_t2 := $(OBJS) $(OBJS_l2) preauth_test.o
OBJS += $(CONFIG_MAIN).o

ifdef CONFIG_PRIVSEP
OBJS_priv += $(OBJS_d) ../src/drivers/drivers.o
OBJS_priv += $(OBJS_l2)
OBJS_priv += ../src/utils/os_$(CONFIG_OS).o
OBJS_priv += ../src/utils/$(CONFIG_ELOOP).o
OBJS_priv += ../src/utils/common.o
OBJS_priv += ../src/utils/wpa_debug.o
OBJS_priv += ../src/utils/wpabuf.o
OBJS_priv += wpa_priv.o
ifdef CONFIG_DRIVER_TEST
OBJS_priv += $(SHA1OBJS)
OBJS_priv += $(MD5OBJS)
ifeq ($(CONFIG_TLS), openssl)
OBJS_priv += ../src/crypto/crypto_openssl.o
endif
ifeq ($(CONFIG_TLS), gnutls)
OBJS_priv += ../src/crypto/crypto_gnutls.o
endif
ifeq ($(CONFIG_TLS), nss)
OBJS_priv += ../src/crypto/crypto_nss.o
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
OBJS_priv += ../src/crypto/crypto_libtomcrypt.o
else
OBJS_priv += ../src/crypto/crypto_internal.o
endif
endif
endif # CONFIG_DRIVER_TEST
OBJS += ../src/l2_packet/l2_packet_privsep.o
OBJS += ../src/drivers/driver_privsep.o
EXTRA_progs += wpa_priv
else
OBJS += $(OBJS_d) ../src/drivers/drivers.o
OBJS += $(OBJS_l2)
endif

ifdef CONFIG_NDIS_EVENTS_INTEGRATED
CFLAGS += -DCONFIG_NDIS_EVENTS_INTEGRATED
OBJS += ../src/drivers/ndis_events.o
EXTRALIBS += -loleaut32 -lole32 -luuid
ifdef PLATFORMSDKLIB
EXTRALIBS += $(PLATFORMSDKLIB)/WbemUuid.Lib
else
EXTRALIBS += WbemUuid.Lib
endif
endif

ifndef LDO
LDO=$(CC)
endif

dynamic_eap_methods: $(EAPDYN)

../src/drivers/build.wpa_supplicant:
	@if [ -f ../src/drivers/build.hostapd ]; then \
		$(MAKE) -C ../src/drivers clean; \
	fi
	@touch ../src/drivers/build.wpa_supplicant

BCHECK=../src/drivers/build.wpa_supplicant

wpa_priv: $(BCHECK) $(OBJS_priv)
	$(LDO) $(LDFLAGS) -o wpa_priv $(OBJS_priv) $(LIBS)

wpa_supplicant: .config $(BCHECK) $(OBJS) $(EXTRA_progs)
	$(LDO) $(LDFLAGS) -o wpa_supplicant $(OBJS) $(LIBS) $(EXTRALIBS)

eapol_test: .config $(OBJS_t)
	$(LDO) $(LDFLAGS) -o eapol_test $(OBJS_t) $(LIBS)

preauth_test: .config $(OBJS_t2) 
	$(LDO) $(LDFLAGS) -o preauth_test $(OBJS_t2) $(LIBS)

wpa_passphrase: $(OBJS_p)
	$(LDO) $(LDFLAGS) -o wpa_passphrase $(OBJS_p) $(LIBS_p)

wpa_cli: $(OBJS_c)
	$(LDO) $(LDFLAGS) -o wpa_cli $(OBJS_c) $(LIBS_c)

link_test: $(OBJS) $(OBJS_h) tests/link_test.o
	$(LDO) $(LDFLAGS) -o link_test $(OBJS) $(OBJS_h) tests/link_test.o $(LIBS)

test_wpa: $(OBJS_wpa) $(OBJS_h)
	$(LDO) $(LDFLAGS) -o test_wpa $(OBJS_wpa) $(LIBS)

win_if_list: win_if_list.c
	$(LDO) $(LDFLAGS) -o $@ win_if_list.c $(CFLAGS) $(LIBS_w)

eap_psk.so: ../src/eap_peer/eap_psk.c ../src/eap_common/eap_psk_common.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_psk_register=eap_peer_method_dynamic_init

eap_pax.so: ../src/eap_peer/eap_pax.c ../src/eap_common/eap_pax_common.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_pax_register=eap_peer_method_dynamic_init

eap_sake.so: ../src/eap_peer/eap_sake.c ../src/eap_common/eap_sake_common.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_sake_register=eap_peer_method_dynamic_init

eap_wsc.so: ../src/eap_peer/eap_wsc.c ../src/eap_common/eap_wsc_common.c ../src/wps/wps.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_wsc_register=eap_peer_method_dynamic_init

eap_ikev2.so: ../src/eap_peer/eap_ikev2.c ../src/eap_peer/ikev2.c ../src/eap_common/eap_ikev2_common.o ../src/eap_common/ikev2_common.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_ikev2_register=eap_peer_method_dynamic_init

%.so: %.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $< \
		-D$(*F:eap_%=eap_peer_%)_register=eap_peer_method_dynamic_init

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<

wpa_supplicant.exe: wpa_supplicant
	mv -f $< $@
wpa_cli.exe: wpa_cli
	mv -f $< $@
wpa_passphrase.exe: wpa_passphrase
	mv -f $< $@
win_if_list.exe: win_if_list
	mv -f $< $@
eapol_test.exe: eapol_test
	mv -f $< $@

WINALL=wpa_supplicant.exe wpa_cli.exe wpa_passphrase.exe win_if_list.exe

windows-bin: $(WINALL)
	$(STRIP) $(WINALL)

wpa_gui/Makefile:
	qmake -o wpa_gui/Makefile wpa_gui/wpa_gui.pro 

wpa_gui: wpa_gui/Makefile
	$(MAKE) -C wpa_gui

wpa_gui-qt4/Makefile:
	qmake -o wpa_gui-qt4/Makefile wpa_gui-qt4/wpa_gui.pro 

wpa_gui-qt4/lang/wpa_gui_de.qm: wpa_gui-qt4/lang/wpa_gui_de.ts
	lrelease wpa_gui-qt4/wpa_gui.pro

wpa_gui-qt4: wpa_gui-qt4/Makefile wpa_gui-qt4/lang/wpa_gui_de.qm
	$(MAKE) -C wpa_gui-qt4

TEST_EAP_SIM_COMMON_OBJS = $(SHA1OBJS) $(MD5OBJS) \
	../src/utils/common.o ../src/utils/os_unix.o \
	../src/utils/wpa_debug.o $(AESOBJS) \
	tests/test_eap_sim_common.o
test-eap_sim_common: $(TEST_EAP_SIM_COMMON_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_EAP_SIM_COMMON_OBJS) $(LIBS)
	./test-eap_sim_common
	rm test-eap_sim_common

tests: test-eap_sim_common

clean:
	$(MAKE) -C ../src clean
	$(MAKE) -C dbus clean
	rm -f core *~ *.o *.d eap_*.so $(ALL) $(WINALL) eapol_test preauth_test
	rm -f wpa_priv

-include $(OBJS:%.o=%.d)
