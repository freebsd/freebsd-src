ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

export LIBDIR ?= /usr/local/lib/
export INCDIR ?= /usr/local/include/
export BINDIR ?= /usr/local/sbin/
PKG_CONFIG ?= pkg-config

CFLAGS += $(EXTRA_CFLAGS)
CFLAGS += -I$(abspath ../src)
CFLAGS += -I$(abspath ../src/utils)

-include .config

ifdef CONFIG_TESTING_OPTIONS
CFLAGS += -DCONFIG_TESTING_OPTIONS
CONFIG_WPS_TESTING=y
CONFIG_TDLS_TESTING=y
endif

BINALL=wpa_supplicant wpa_cli

ifndef CONFIG_NO_WPA_PASSPHRASE
BINALL += wpa_passphrase
endif

ALL = $(BINALL)
ALL += systemd/wpa_supplicant.service
ALL += systemd/wpa_supplicant@.service
ALL += systemd/wpa_supplicant-nl80211@.service
ALL += systemd/wpa_supplicant-wired@.service
ALL += dbus/fi.epitest.hostap.WPASupplicant.service
ALL += dbus/fi.w1.wpa_supplicant1.service
ifdef CONFIG_BUILD_WPA_CLIENT_SO
ALL += libwpa_client.so
endif


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

$(DESTDIR)$(BINDIR)/%: %
	install -D $(<) $(@)

install: $(addprefix $(DESTDIR)$(BINDIR)/,$(BINALL))
	$(MAKE) -C ../src install
ifdef CONFIG_BUILD_WPA_CLIENT_SO
	install -m 0644 -D libwpa_client.so $(DESTDIR)/$(LIBDIR)/libwpa_client.so
	install -m 0644 -D ../src/common/wpa_ctrl.h $(DESTDIR)/$(INCDIR)/wpa_ctrl.h
endif

ifdef CONFIG_FIPS
CONFIG_NO_RANDOM_POOL=
CONFIG_OPENSSL_CMAC=y
endif

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
OBJS_c += ../src/utils/wpa_debug.o
OBJS_c += ../src/utils/common.o
OBJS += wmm_ac.o

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
OBJS_priv += ../src/utils/trace.o
LIBCTRL += ../src/utils/trace.o
LIBCTRLSO += ../src/utils/trace.c
LDFLAGS += -rdynamic
CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
CFLAGS += -DPACKAGE="wpa_supplicant" -DWPA_TRACE_BFD
LIBS += -lbfd -ldl -liberty -lz
LIBS_p += -lbfd -ldl -liberty -lz
LIBS_c += -lbfd -ldl -liberty -lz
endif
endif

ifndef CONFIG_ELOOP
CONFIG_ELOOP=eloop
endif
OBJS += ../src/utils/$(CONFIG_ELOOP).o
OBJS_c += ../src/utils/$(CONFIG_ELOOP).o

ifndef CONFIG_OSX
ifeq ($(CONFIG_ELOOP), eloop)
# Using glibc < 2.17 requires -lrt for clock_gettime()
# OS X has an alternate implementation
LIBS += -lrt
LIBS_c += -lrt
LIBS_p += -lrt
endif
endif

ifdef CONFIG_ELOOP_POLL
CFLAGS += -DCONFIG_ELOOP_POLL
endif

ifdef CONFIG_ELOOP_EPOLL
CFLAGS += -DCONFIG_ELOOP_EPOLL
endif

ifdef CONFIG_EAPOL_TEST
CFLAGS += -Werror -DEAPOL_TEST
endif

ifdef CONFIG_CODE_COVERAGE
CFLAGS += -O0 -fprofile-arcs -ftest-coverage
LIBS += -lgcov
LIBS_c += -lgcov
LIBS_p += -lgcov
endif

ifdef CONFIG_HT_OVERRIDES
CFLAGS += -DCONFIG_HT_OVERRIDES
endif

ifdef CONFIG_VHT_OVERRIDES
CFLAGS += -DCONFIG_VHT_OVERRIDES
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

ifdef CONFIG_SUITEB
CFLAGS += -DCONFIG_SUITEB
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_SUITEB192
CFLAGS += -DCONFIG_SUITEB192
NEED_SHA384=y
endif

ifdef CONFIG_IEEE80211W
CFLAGS += -DCONFIG_IEEE80211W
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_IEEE80211R
CFLAGS += -DCONFIG_IEEE80211R
OBJS += ../src/rsn_supp/wpa_ft.o
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_MESH
NEED_80211_COMMON=y
NEED_SHA256=y
NEED_AES_SIV=y
NEED_AES_OMAC1=y
NEED_AES_CTR=y
CONFIG_SAE=y
CONFIG_AP=y
CFLAGS += -DCONFIG_MESH
OBJS += mesh.o
OBJS += mesh_mpm.o
OBJS += mesh_rsn.o
endif

ifdef CONFIG_SAE
CFLAGS += -DCONFIG_SAE
OBJS += ../src/common/sae.o
NEED_ECC=y
NEED_DH_GROUPS=y
endif

ifdef CONFIG_WNM
CFLAGS += -DCONFIG_WNM
OBJS += wnm_sta.o
endif

ifdef CONFIG_TDLS
CFLAGS += -DCONFIG_TDLS
OBJS += ../src/rsn_supp/tdls.o
NEED_SHA256=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_TDLS_TESTING
CFLAGS += -DCONFIG_TDLS_TESTING
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
CFLAGS += -DCONFIG_NO_WPA
endif

ifdef CONFIG_IBSS_RSN
NEED_RSN_AUTHENTICATOR=y
CFLAGS += -DCONFIG_IBSS_RSN
OBJS += ibss_rsn.o
endif

ifdef CONFIG_P2P
OBJS += p2p_supplicant.o
OBJS += p2p_supplicant_sd.o
OBJS += ../src/p2p/p2p.o
OBJS += ../src/p2p/p2p_utils.o
OBJS += ../src/p2p/p2p_parse.o
OBJS += ../src/p2p/p2p_build.o
OBJS += ../src/p2p/p2p_go_neg.o
OBJS += ../src/p2p/p2p_sd.o
OBJS += ../src/p2p/p2p_pd.o
OBJS += ../src/p2p/p2p_invitation.o
OBJS += ../src/p2p/p2p_dev_disc.o
OBJS += ../src/p2p/p2p_group.o
OBJS += ../src/ap/p2p_hostapd.o
OBJS += ../src/utils/bitfield.o
CFLAGS += -DCONFIG_P2P
NEED_GAS=y
NEED_OFFCHANNEL=y
CONFIG_WPS=y
CONFIG_AP=y
ifdef CONFIG_P2P_STRICT
CFLAGS += -DCONFIG_P2P_STRICT
endif
endif

ifdef CONFIG_WIFI_DISPLAY
CFLAGS += -DCONFIG_WIFI_DISPLAY
OBJS += wifi_display.o
endif

ifdef CONFIG_HS20
OBJS += hs20_supplicant.o
CFLAGS += -DCONFIG_HS20
CONFIG_INTERWORKING=y
NEED_AES_OMAC1=y
endif

ifdef CONFIG_INTERWORKING
OBJS += interworking.o
CFLAGS += -DCONFIG_INTERWORKING
NEED_GAS=y
endif

ifdef CONFIG_NO_ROAMING
CFLAGS += -DCONFIG_NO_ROAMING
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

ifdef CONFIG_ERP
CFLAGS += -DCONFIG_ERP
NEED_SHA256=y
NEED_HMAC_SHA256_KDF=y
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

ifdef CONFIG_EAP_UNAUTH_TLS
# EAP-UNAUTH-TLS
CFLAGS += -DEAP_UNAUTH_TLS
ifndef CONFIG_EAP_TLS
OBJS += ../src/eap_peer/eap_tls.o
OBJS_h += ../src/eap_server/eap_server_tls.o
TLS_FUNCS=y
endif
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
TLS_FUNCS=y
ifndef CONFIG_FIPS
MS_FUNCS=y
CHAP=y
endif
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

ifdef CONFIG_EAP_PROXY
CFLAGS += -DCONFIG_EAP_PROXY
OBJS += ../src/eap_peer/eap_proxy_$(CONFIG_EAP_PROXY).o
include eap_proxy_$(CONFIG_EAP_PROXY).mak
CONFIG_IEEE8021X_EAPOL=y
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

ifdef CONFIG_EAP_PWD
CFLAGS += -DEAP_PWD
OBJS += ../src/eap_peer/eap_pwd.o ../src/eap_common/eap_pwd_common.o
OBJS_h += ../src/eap_server/eap_server_pwd.o
CONFIG_IEEE8021X_EAPOL=y
NEED_SHA256=y
endif

ifdef CONFIG_EAP_EKE
# EAP-EKE
ifeq ($(CONFIG_EAP_EKE), dyn)
CFLAGS += -DEAP_EKE_DYNAMIC
EAPDYN += ../src/eap_peer/eap_eke.so
else
CFLAGS += -DEAP_EKE
OBJS += ../src/eap_peer/eap_eke.o ../src/eap_common/eap_eke_common.o
OBJS_h += ../src/eap_server/eap_server_eke.o
endif
CONFIG_IEEE8021X_EAPOL=y
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
NEED_SHA256=y
NEED_AES_CBC=y
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
NEED_AES_CBC=y
NEED_MODEXP=y

ifdef CONFIG_WPS_NFC
CFLAGS += -DCONFIG_WPS_NFC
OBJS += ../src/wps/ndef.o
NEED_WPS_OOB=y
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

ifdef CONFIG_WPS_STRICT
CFLAGS += -DCONFIG_WPS_STRICT
OBJS += ../src/wps/wps_validate.o
endif

ifdef CONFIG_WPS_TESTING
CFLAGS += -DCONFIG_WPS_TESTING
endif

ifdef CONFIG_WPS_REG_DISABLE_OPEN
CFLAGS += -DCONFIG_WPS_REG_DISABLE_OPEN
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

ifdef CONFIG_MACSEC
CFLAGS += -DCONFIG_MACSEC
NEED_AES_ENCBLOCK=y
NEED_AES_UNWRAP=y
NEED_AES_WRAP=y
NEED_AES_OMAC1=y
OBJS += wpas_kay.o
OBJS += ../src/pae/ieee802_1x_cp.o
OBJS += ../src/pae/ieee802_1x_kay.o
OBJS += ../src/pae/ieee802_1x_key.o
OBJS += ../src/pae/ieee802_1x_secy_ops.o
endif

ifdef CONFIG_AP
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
OBJS += ../src/ap/ieee802_11_shared.o
OBJS += ../src/ap/drv_callbacks.o
OBJS += ../src/ap/ap_drv_ops.o
OBJS += ../src/ap/beacon.o
OBJS += ../src/ap/bss_load.o
OBJS += ../src/ap/eap_user_db.o
ifdef CONFIG_IEEE80211N
OBJS += ../src/ap/ieee802_11_ht.o
ifdef CONFIG_IEEE80211AC
OBJS += ../src/ap/ieee802_11_vht.o
endif
endif
ifdef CONFIG_WNM
OBJS += ../src/ap/wnm_ap.o
endif
ifdef CONFIG_CTRL_IFACE
OBJS += ../src/ap/ctrl_iface_ap.o
endif

CFLAGS += -DEAP_SERVER -DEAP_SERVER_IDENTITY
OBJS += ../src/eap_server/eap_server.o
OBJS += ../src/eap_server/eap_server_identity.o
OBJS += ../src/eap_server/eap_server_methods.o

ifdef CONFIG_IEEE80211N
CFLAGS += -DCONFIG_IEEE80211N
ifdef CONFIG_IEEE80211AC
CFLAGS += -DCONFIG_IEEE80211AC
endif
endif

ifdef NEED_AP_MLME
OBJS += ../src/ap/wmm.o
OBJS += ../src/ap/ap_list.o
OBJS += ../src/ap/ieee802_11.o
OBJS += ../src/ap/hw_features.o
OBJS += ../src/ap/dfs.o
CFLAGS += -DNEED_AP_MLME
endif
ifdef CONFIG_WPS
CFLAGS += -DEAP_SERVER_WSC
OBJS += ../src/ap/wps_hostapd.o
OBJS += ../src/eap_server/eap_server_wsc.o
endif
ifdef CONFIG_INTERWORKING
OBJS += ../src/ap/gas_serv.o
endif
ifdef CONFIG_HS20
OBJS += ../src/ap/hs20.o
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
NEED_AES_ENCBLOCK=y
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
ifndef CONFIG_FIPS
NEED_TLS_PRF=y
NEED_SHA1=y
NEED_MD5=y
endif
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifdef CONFIG_TLSV11
CFLAGS += -DCONFIG_TLSV11
endif

ifdef CONFIG_TLSV12
CFLAGS += -DCONFIG_TLSV12
NEED_SHA256=y
endif

ifeq ($(CONFIG_TLS), openssl)
ifdef TLS_FUNCS
CFLAGS += -DEAP_TLS_OPENSSL
OBJS += ../src/crypto/tls_openssl.o
LIBS += -lssl
endif
OBJS += ../src/crypto/crypto_openssl.o
OBJS_p += ../src/crypto/crypto_openssl.o
OBJS_priv += ../src/crypto/crypto_openssl.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_openssl.o
endif
NEED_SHA256=y
NEED_TLS_PRF_SHA256=y
LIBS += -lcrypto
LIBS_p += -lcrypto
ifdef CONFIG_TLS_ADD_DL
LIBS += -ldl
LIBS_p += -ldl
endif
endif

ifeq ($(CONFIG_TLS), gnutls)
ifdef TLS_FUNCS
OBJS += ../src/crypto/tls_gnutls.o
LIBS += -lgnutls -lgpg-error
endif
OBJS += ../src/crypto/crypto_gnutls.o
OBJS_p += ../src/crypto/crypto_gnutls.o
OBJS_priv += ../src/crypto/crypto_gnutls.o
ifdef NEED_FIPS186_2_PRF
OBJS += ../src/crypto/fips_prf_internal.o
SHA1OBJS += ../src/crypto/sha1-internal.o
endif
LIBS += -lgcrypt
LIBS_p += -lgcrypt
CONFIG_INTERNAL_SHA256=y
CONFIG_INTERNAL_RC4=y
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
ifdef CONFIG_TLSV12
NEED_TLS_PRF_SHA256=y
endif
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

ifneq ($(CONFIG_TLS), openssl)
NEED_INTERNAL_AES_WRAP=y
endif
ifdef CONFIG_OPENSSL_INTERNAL_AES_WRAP
# Seems to be needed at least with BoringSSL
NEED_INTERNAL_AES_WRAP=y
CFLAGS += -DCONFIG_OPENSSL_INTERNAL_AES_WRAP
endif
ifdef CONFIG_FIPS
# Have to use internal AES key wrap routines to use OpenSSL EVP since the
# OpenSSL AES_wrap_key()/AES_unwrap_key() API is not available in FIPS mode.
NEED_INTERNAL_AES_WRAP=y
endif

ifdef NEED_INTERNAL_AES_WRAP
AESOBJS += ../src/crypto/aes-unwrap.o
endif
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
ifdef CONFIG_OPENSSL_CMAC
CFLAGS += -DCONFIG_OPENSSL_CMAC
else
AESOBJS += ../src/crypto/aes-omac1.o
endif
endif
ifdef NEED_AES_SIV
AESOBJS += ../src/crypto/aes-siv.o
endif
ifdef NEED_AES_WRAP
NEED_AES_ENC=y
ifdef NEED_INTERNAL_AES_WRAP
AESOBJS += ../src/crypto/aes-wrap.o
endif
endif
ifdef NEED_AES_CBC
NEED_AES_ENC=y
ifneq ($(CONFIG_TLS), openssl)
AESOBJS += ../src/crypto/aes-cbc.o
endif
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
ifneq ($(CONFIG_TLS), openssl)
SHA1OBJS += ../src/crypto/sha1.o
endif
SHA1OBJS += ../src/crypto/sha1-prf.o
ifdef CONFIG_INTERNAL_SHA1
SHA1OBJS += ../src/crypto/sha1-internal.o
ifdef NEED_FIPS186_2_PRF
SHA1OBJS += ../src/crypto/fips_prf_internal.o
endif
endif
ifdef CONFIG_NO_WPA_PASSPHRASE
CFLAGS += -DCONFIG_NO_PBKDF2
else
ifneq ($(CONFIG_TLS), openssl)
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

ifndef CONFIG_FIPS
ifneq ($(CONFIG_TLS), openssl)
MD5OBJS += ../src/crypto/md5.o
endif
endif
ifdef NEED_MD5
ifdef CONFIG_INTERNAL_MD5
MD5OBJS += ../src/crypto/md5-internal.o
endif
OBJS += $(MD5OBJS)
OBJS_p += $(MD5OBJS)
OBJS_priv += $(MD5OBJS)
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

SHA256OBJS = # none by default
ifdef NEED_SHA256
CFLAGS += -DCONFIG_SHA256
ifneq ($(CONFIG_TLS), openssl)
SHA256OBJS += ../src/crypto/sha256.o
endif
SHA256OBJS += ../src/crypto/sha256-prf.o
ifdef CONFIG_INTERNAL_SHA256
SHA256OBJS += ../src/crypto/sha256-internal.o
endif
ifdef NEED_TLS_PRF_SHA256
SHA256OBJS += ../src/crypto/sha256-tlsprf.o
endif
ifdef NEED_HMAC_SHA256_KDF
OBJS += ../src/crypto/sha256-kdf.o
endif
OBJS += $(SHA256OBJS)
endif
ifdef NEED_SHA384
CFLAGS += -DCONFIG_SHA384
OBJS += ../src/crypto/sha384-prf.o
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
OBJS += ../src/crypto/random.o
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
ifeq ($(CONFIG_CTRL_IFACE), udp6)
CONFIG_CTRL_IFACE=udp
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_IPV6
endif
ifeq ($(CONFIG_CTRL_IFACE), named_pipe)
CFLAGS += -DCONFIG_CTRL_IFACE_NAMED_PIPE
endif
ifeq ($(CONFIG_CTRL_IFACE), udp-remote)
CONFIG_CTRL_IFACE=udp
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_REMOTE
endif
ifeq ($(CONFIG_CTRL_IFACE), udp6-remote)
CONFIG_CTRL_IFACE=udp
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_REMOTE
CFLAGS += -DCONFIG_CTRL_IFACE_UDP_IPV6
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
DBUS_LIBS := $(shell $(PKG_CONFIG) --libs dbus-1)
endif
ifndef DBUS_INCLUDE
DBUS_INCLUDE := $(shell $(PKG_CONFIG) --cflags dbus-1)
endif
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
ifdef CONFIG_P2P
DBUS_OBJS += dbus/dbus_new_handlers_p2p.o
endif
ifndef DBUS_LIBS
DBUS_LIBS := $(shell $(PKG_CONFIG) --libs dbus-1)
endif
ifndef DBUS_INCLUDE
DBUS_INCLUDE := $(shell $(PKG_CONFIG) --cflags dbus-1)
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
OBJS_c += ../src/utils/edit_readline.o
LIBS_c += -lncurses -lreadline
else
ifdef CONFIG_WPA_CLI_EDIT
OBJS_c += ../src/utils/edit.o
else
OBJS_c += ../src/utils/edit_simple.o
endif
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
OBJS += sme.o
CFLAGS += -DCONFIG_SME
endif

OBJS += ../src/common/ieee802_11_common.o
OBJS += ../src/common/hw_features_common.o

ifdef NEED_EAP_COMMON
OBJS += ../src/eap_common/eap_common.o
endif

ifndef CONFIG_MAIN
CONFIG_MAIN=main
endif

ifdef CONFIG_DEBUG_SYSLOG
CFLAGS += -DCONFIG_DEBUG_SYSLOG
ifdef CONFIG_DEBUG_SYSLOG_FACILITY
CFLAGS += -DLOG_HOSTAPD="$(CONFIG_DEBUG_SYSLOG_FACILITY)"
endif
endif

ifdef CONFIG_DEBUG_LINUX_TRACING
CFLAGS += -DCONFIG_DEBUG_LINUX_TRACING
endif

ifdef CONFIG_DEBUG_FILE
CFLAGS += -DCONFIG_DEBUG_FILE
endif

ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
CFLAGS += -DCONFIG_DELAYED_MIC_ERROR_REPORT
endif

ifdef CONFIG_FIPS
CFLAGS += -DCONFIG_FIPS
ifneq ($(CONFIG_TLS), openssl)
$(error CONFIG_FIPS=y requires CONFIG_TLS=openssl)
endif
endif

OBJS += $(SHA1OBJS) $(DESOBJS)

OBJS_p += $(SHA1OBJS)
OBJS_p += $(SHA256OBJS)
OBJS_priv += $(SHA1OBJS)

ifdef CONFIG_BGSCAN_SIMPLE
CFLAGS += -DCONFIG_BGSCAN_SIMPLE
OBJS += bgscan_simple.o
NEED_BGSCAN=y
endif

ifdef CONFIG_BGSCAN_LEARN
CFLAGS += -DCONFIG_BGSCAN_LEARN
OBJS += bgscan_learn.o
NEED_BGSCAN=y
endif

ifdef NEED_BGSCAN
CFLAGS += -DCONFIG_BGSCAN
OBJS += bgscan.o
endif

ifdef CONFIG_AUTOSCAN_EXPONENTIAL
CFLAGS += -DCONFIG_AUTOSCAN_EXPONENTIAL
OBJS += autoscan_exponential.o
NEED_AUTOSCAN=y
endif

ifdef CONFIG_AUTOSCAN_PERIODIC
CFLAGS += -DCONFIG_AUTOSCAN_PERIODIC
OBJS += autoscan_periodic.o
NEED_AUTOSCAN=y
endif

ifdef NEED_AUTOSCAN
CFLAGS += -DCONFIG_AUTOSCAN
OBJS += autoscan.o
endif

ifdef CONFIG_EXT_PASSWORD_TEST
OBJS += ../src/utils/ext_password_test.o
CFLAGS += -DCONFIG_EXT_PASSWORD_TEST
NEED_EXT_PASSWORD=y
endif

ifdef NEED_EXT_PASSWORD
OBJS += ../src/utils/ext_password.o
CFLAGS += -DCONFIG_EXT_PASSWORD
endif

ifdef NEED_GAS
OBJS += ../src/common/gas.o
OBJS += gas_query.o
CFLAGS += -DCONFIG_GAS
NEED_OFFCHANNEL=y
endif

ifdef NEED_OFFCHANNEL
OBJS += offchannel.o
CFLAGS += -DCONFIG_OFFCHANNEL
endif

ifdef CONFIG_MODULE_TESTS
CFLAGS += -DCONFIG_MODULE_TESTS
OBJS += wpas_module_tests.o
OBJS += ../src/utils/utils_module_tests.o
OBJS += ../src/common/common_module_tests.o
OBJS += ../src/crypto/crypto_module_tests.o
ifdef CONFIG_WPS
OBJS += ../src/wps/wps_module_tests.o
endif
ifndef CONFIG_P2P
OBJS += ../src/utils/bitfield.o
endif
endif

OBJS += ../src/drivers/driver_common.o
OBJS_priv += ../src/drivers/driver_common.o

OBJS_wpa_rm := ctrl_iface.o ctrl_iface_unix.o
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

OBJS_nfc := $(OBJS) $(OBJS_l2) nfc_pw_token.o
OBJS_nfc += $(OBJS_d) ../src/drivers/drivers.o

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
ifdef CONFIG_DRIVER_NL80211
OBJS_priv += ../src/common/ieee802_11_common.o
endif
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

ifdef CONFIG_FST
CFLAGS += -DCONFIG_FST
ifdef CONFIG_FST_TEST
CFLAGS += -DCONFIG_FST_TEST
endif
FST_OBJS += ../src/fst/fst.o
FST_OBJS += ../src/fst/fst_session.o
FST_OBJS += ../src/fst/fst_iface.o
FST_OBJS += ../src/fst/fst_group.o
FST_OBJS += ../src/fst/fst_ctrl_aux.o
ifdef CONFIG_CTRL_IFACE
FST_OBJS += ../src/fst/fst_ctrl_iface.o
endif
OBJS += $(FST_OBJS)
OBJS_t += $(FST_OBJS)
OBJS_t2 += $(FST_OBJS)
endif

ifndef LDO
LDO=$(CC)
endif

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif
ifeq ($(QUIET), 1)
Q=@
E=true
endif

dynamic_eap_methods: $(EAPDYN)

../src/drivers/build.wpa_supplicant:
	@if [ -f ../src/drivers/build.hostapd ]; then \
		$(MAKE) -C ../src/drivers clean; \
	fi
	@touch ../src/drivers/build.wpa_supplicant

BCHECK=../src/drivers/build.wpa_supplicant

wpa_priv: $(BCHECK) $(OBJS_priv)
	$(Q)$(LDO) $(LDFLAGS) -o wpa_priv $(OBJS_priv) $(LIBS)
	@$(E) "  LD " $@

$(OBJS_c) $(OBJS_t) $(OBJS_t2) $(OBJS) $(BCHECK) $(EXTRA_progs): .config

wpa_supplicant: $(BCHECK) $(OBJS) $(EXTRA_progs)
	$(Q)$(LDO) $(LDFLAGS) -o wpa_supplicant $(OBJS) $(LIBS) $(EXTRALIBS)
	@$(E) "  LD " $@

eapol_test: $(OBJS_t)
	$(Q)$(LDO) $(LDFLAGS) -o eapol_test $(OBJS_t) $(LIBS)
	@$(E) "  LD " $@

preauth_test: $(OBJS_t2)
	$(Q)$(LDO) $(LDFLAGS) -o preauth_test $(OBJS_t2) $(LIBS)
	@$(E) "  LD " $@

wpa_passphrase: $(OBJS_p)
	$(Q)$(LDO) $(LDFLAGS) -o wpa_passphrase $(OBJS_p) $(LIBS_p)
	@$(E) "  LD " $@

wpa_cli: $(OBJS_c)
	$(Q)$(LDO) $(LDFLAGS) -o wpa_cli $(OBJS_c) $(LIBS_c)
	@$(E) "  LD " $@

LIBCTRL += ../src/common/wpa_ctrl.o
LIBCTRL += ../src/utils/os_$(CONFIG_OS).o
LIBCTRL += ../src/utils/wpa_debug.o
LIBCTRLSO += ../src/common/wpa_ctrl.c
LIBCTRLSO += ../src/utils/os_$(CONFIG_OS).c
LIBCTRLSO += ../src/utils/wpa_debug.c

libwpa_client.a: $(LIBCTRL)
	$(Q)rm -f $@
	$(Q)$(AR) crs $@ $?
	@$(E) "  AR " $@

libwpa_client.so: $(LIBCTRLSO)
	@$(E) "  CC  $@ ($^)"
	$(Q)$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -fPIC $^

link_test: $(OBJS) $(OBJS_h) tests/link_test.o
	$(Q)$(LDO) $(LDFLAGS) -o link_test $(OBJS) $(OBJS_h) tests/link_test.o $(LIBS)
	@$(E) "  LD " $@

test_wpa: $(OBJS_wpa) $(OBJS_h)
	$(Q)$(LDO) $(LDFLAGS) -o test_wpa $(OBJS_wpa) $(LIBS)
	@$(E) "  LD " $@

nfc_pw_token: $(OBJS_nfc)
	$(Q)$(LDO) $(LDFLAGS) -o nfc_pw_token $(OBJS_nfc) $(LIBS)
	@$(E) "  LD " $@

win_if_list: win_if_list.c
	$(Q)$(LDO) $(LDFLAGS) -o $@ win_if_list.c $(CFLAGS) $(LIBS_w)
	@$(E) "  LD " $@

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

eap_eke.so: ../src/eap_peer/eap_eke.c ../src/eap_common/eap_eke_common.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_eke_register=eap_peer_method_dynamic_init

%.so: %.c
	$(CC) $(LDFLAGS) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $< \
		-D$(*F:eap_%=eap_peer_%)_register=eap_peer_method_dynamic_init

ifdef CONFIG_CODE_COVERAGE
%.o: %.c
	@$(E) "  CC " $<
	$(Q)cd $(dir $@); $(CC) -c -o $(notdir $@) $(CFLAGS) $(notdir $<)
else
%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<
endif

%.service: %.service.in
	$(Q)sed -e 's|\@BINDIR\@|$(BINDIR)|g' $< >$@
	@$(E) "  sed" $<

%@.service: %.service.arg.in
	$(Q)sed -e 's|\@BINDIR\@|$(BINDIR)|g' $< >$@
	@$(E) "  sed" $<

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

wpa_gui:
	@echo "wpa_gui has been removed - see wpa_gui-qt4 for replacement"

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

FIPSDIR=/usr/local/ssl/fips-2.0
FIPSLD=$(FIPSDIR)/bin/fipsld
fips:
	$(MAKE) CC=$(FIPSLD) FIPSLD_CC="$(CC)"

lcov-html: wpa_supplicant.gcda
	lcov -c -d .. > lcov.info
	genhtml lcov.info --output-directory lcov-html

clean:
	$(MAKE) -C ../src clean
	$(MAKE) -C dbus clean
	rm -f core *~ *.o *.d *.gcno *.gcda *.gcov
	rm -f eap_*.so $(ALL) $(WINALL) eapol_test preauth_test
	rm -f wpa_priv
	rm -f nfc_pw_token
	rm -f lcov.info
	rm -rf lcov-html
	rm -f libwpa_client.a
	rm -f libwpa_client.so

-include $(OBJS:%.o=%.d)
