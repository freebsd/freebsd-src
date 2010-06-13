ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
CFLAGS += -DHOSTAPD_DUMP_STATE

CFLAGS += -I../src
CFLAGS += -I../src/crypto
CFLAGS += -I../src/utils
CFLAGS += -I../src/common

# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
# CFLAGS += -DUSE_KERNEL_HEADERS -I/usr/src/linux/include

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

ifdef CONFIG_NATIVE_WINDOWS
CFLAGS += -DCONFIG_NATIVE_WINDOWS
LIBS += -lws2_32
endif

OBJS =	hostapd.o ieee802_1x.o eapol_sm.o \
	ieee802_11.o config.o ieee802_11_auth.o accounting.o \
	sta_info.o wpa.o ctrl_iface.o \
	drivers.o preauth.o pmksa_cache.o beacon.o \
	hw_features.o wme.o ap_list.o \
	mlme.o vlan_init.o wpa_auth_ie.o

OBJS += ../src/utils/eloop.o
OBJS += ../src/utils/common.o
OBJS += ../src/utils/wpa_debug.o
OBJS += ../src/utils/wpabuf.o
OBJS += ../src/utils/os_$(CONFIG_OS).o
OBJS += ../src/utils/ip_addr.o

OBJS += ../src/common/ieee802_11_common.o
OBJS += ../src/common/wpa_common.o

OBJS += ../src/radius/radius.o
OBJS += ../src/radius/radius_client.o

OBJS += ../src/crypto/md5.o
OBJS += ../src/crypto/rc4.o
OBJS += ../src/crypto/md4.o
OBJS += ../src/crypto/sha1.o
OBJS += ../src/crypto/des.o
OBJS += ../src/crypto/aes_wrap.o
OBJS += ../src/crypto/aes.o

HOBJS=../src/hlr_auc_gw/hlr_auc_gw.o ../src/utils/common.o ../src/utils/wpa_debug.o ../src/utils/os_$(CONFIG_OS).o ../src/hlr_auc_gw/milenage.o ../src/crypto/aes_wrap.o ../src/crypto/aes.o

CFLAGS += -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX

ifdef CONFIG_IAPP
CFLAGS += -DCONFIG_IAPP
OBJS += iapp.o
endif

ifdef CONFIG_RSN_PREAUTH
CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_PEERKEY
CFLAGS += -DCONFIG_PEERKEY
OBJS += peerkey.o
endif

ifdef CONFIG_IEEE80211W
CFLAGS += -DCONFIG_IEEE80211W
NEED_SHA256=y
endif

ifdef CONFIG_IEEE80211R
CFLAGS += -DCONFIG_IEEE80211R
OBJS += wpa_ft.o
NEED_SHA256=y
endif

ifdef CONFIG_IEEE80211N
CFLAGS += -DCONFIG_IEEE80211N
endif

ifdef CONFIG_DRIVER_HOSTAP
CFLAGS += -DCONFIG_DRIVER_HOSTAP
OBJS += driver_hostap.o
endif

ifdef CONFIG_DRIVER_WIRED
CFLAGS += -DCONFIG_DRIVER_WIRED
OBJS += driver_wired.o
endif

ifdef CONFIG_DRIVER_MADWIFI
CFLAGS += -DCONFIG_DRIVER_MADWIFI
OBJS += driver_madwifi.o
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_ATHEROS
CFLAGS += -DCONFIG_DRIVER_ATHEROS
OBJS += driver_atheros.o
CONFIG_L2_PACKET=y
endif

ifdef CONFIG_DRIVER_PRISM54
CFLAGS += -DCONFIG_DRIVER_PRISM54
OBJS += driver_prism54.o
endif

ifdef CONFIG_DRIVER_NL80211
CFLAGS += -DCONFIG_DRIVER_NL80211
OBJS += driver_nl80211.o radiotap.o
LIBS += -lnl
ifdef CONFIG_LIBNL20
LIBS += -lnl-genl
CFLAGS += -DCONFIG_LIBNL20
endif
endif

ifdef CONFIG_DRIVER_BSD
CFLAGS += -DCONFIG_DRIVER_BSD
OBJS += driver_bsd.o
CONFIG_L2_PACKET=y
CONFIG_DNET_PCAP=y
CONFIG_L2_FREEBSD=y
endif

ifdef CONFIG_DRIVER_TEST
CFLAGS += -DCONFIG_DRIVER_TEST
OBJS += driver_test.o
endif

ifdef CONFIG_DRIVER_NONE
CFLAGS += -DCONFIG_DRIVER_NONE
OBJS += driver_none.o
endif

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


ifdef CONFIG_EAP_MD5
CFLAGS += -DEAP_MD5
OBJS += ../src/eap_server/eap_md5.o
CHAP=y
endif

ifdef CONFIG_EAP_TLS
CFLAGS += -DEAP_TLS
OBJS += ../src/eap_server/eap_tls.o
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_PEAP
CFLAGS += -DEAP_PEAP
OBJS += ../src/eap_server/eap_peap.o
OBJS += ../src/eap_common/eap_peap_common.o
TLS_FUNCS=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
CFLAGS += -DEAP_TTLS
OBJS += ../src/eap_server/eap_ttls.o
TLS_FUNCS=y
CHAP=y
endif

ifdef CONFIG_EAP_MSCHAPV2
CFLAGS += -DEAP_MSCHAPv2
OBJS += ../src/eap_server/eap_mschapv2.o
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
CFLAGS += -DEAP_GTC
OBJS += ../src/eap_server/eap_gtc.o
endif

ifdef CONFIG_EAP_SIM
CFLAGS += -DEAP_SIM
OBJS += ../src/eap_server/eap_sim.o
CONFIG_EAP_SIM_COMMON=y
endif

ifdef CONFIG_EAP_AKA
CFLAGS += -DEAP_AKA
OBJS += ../src/eap_server/eap_aka.o
CONFIG_EAP_SIM_COMMON=y
NEED_SHA256=y
endif

ifdef CONFIG_EAP_AKA_PRIME
CFLAGS += -DEAP_AKA_PRIME
endif

ifdef CONFIG_EAP_SIM_COMMON
OBJS += ../src/eap_common/eap_sim_common.o
# Example EAP-SIM/AKA interface for GSM/UMTS authentication. This can be
# replaced with another file implementating the interface specified in
# eap_sim_db.h.
OBJS += ../src/eap_server/eap_sim_db.o
NEED_FIPS186_2_PRF=y
endif

ifdef CONFIG_EAP_PAX
CFLAGS += -DEAP_PAX
OBJS += ../src/eap_server/eap_pax.o ../src/eap_common/eap_pax_common.o
endif

ifdef CONFIG_EAP_PSK
CFLAGS += -DEAP_PSK
OBJS += ../src/eap_server/eap_psk.o ../src/eap_common/eap_psk_common.o
endif

ifdef CONFIG_EAP_SAKE
CFLAGS += -DEAP_SAKE
OBJS += ../src/eap_server/eap_sake.o ../src/eap_common/eap_sake_common.o
endif

ifdef CONFIG_EAP_GPSK
CFLAGS += -DEAP_GPSK
OBJS += ../src/eap_server/eap_gpsk.o ../src/eap_common/eap_gpsk_common.o
ifdef CONFIG_EAP_GPSK_SHA256
CFLAGS += -DEAP_GPSK_SHA256
endif
NEED_SHA256=y
endif

ifdef CONFIG_EAP_VENDOR_TEST
CFLAGS += -DEAP_VENDOR_TEST
OBJS += ../src/eap_server/eap_vendor_test.o
endif

ifdef CONFIG_EAP_FAST
CFLAGS += -DEAP_FAST
OBJS += ../src/eap_server/eap_fast.o
OBJS += ../src/eap_common/eap_fast_common.o
TLS_FUNCS=y
NEED_T_PRF=y
endif

ifdef CONFIG_WPS
CFLAGS += -DCONFIG_WPS -DEAP_WSC
OBJS += ../src/utils/uuid.o
OBJS += wps_hostapd.o
OBJS += ../src/eap_server/eap_wsc.o ../src/eap_common/eap_wsc_common.o
OBJS += ../src/wps/wps.o
OBJS += ../src/wps/wps_common.o
OBJS += ../src/wps/wps_attr_parse.o
OBJS += ../src/wps/wps_attr_build.o
OBJS += ../src/wps/wps_attr_process.o
OBJS += ../src/wps/wps_dev_attr.o
OBJS += ../src/wps/wps_enrollee.o
OBJS += ../src/wps/wps_registrar.o
NEED_DH_GROUPS=y
NEED_SHA256=y
NEED_CRYPTO=y
NEED_BASE64=y

ifdef CONFIG_WPS_UPNP
CFLAGS += -DCONFIG_WPS_UPNP
OBJS += ../src/wps/wps_upnp.o
OBJS += ../src/wps/wps_upnp_ssdp.o
OBJS += ../src/wps/wps_upnp_web.o
OBJS += ../src/wps/wps_upnp_event.o
OBJS += ../src/wps/httpread.o
endif

endif

ifdef CONFIG_EAP_IKEV2
CFLAGS += -DEAP_IKEV2
OBJS += ../src/eap_server/eap_ikev2.o ../src/eap_server/ikev2.o
OBJS += ../src/eap_common/eap_ikev2_common.o ../src/eap_common/ikev2_common.o
NEED_DH_GROUPS=y
NEED_DH_GROUPS_ALL=y
endif

ifdef CONFIG_EAP_TNC
CFLAGS += -DEAP_TNC
OBJS += ../src/eap_server/eap_tnc.o
OBJS += ../src/eap_server/tncs.o
NEED_BASE64=y
ifndef CONFIG_DRIVER_BSD
LIBS += -ldl
endif
endif

# Basic EAP functionality is needed for EAPOL
OBJS += ../src/eap_server/eap.o
OBJS += ../src/eap_common/eap_common.o
OBJS += ../src/eap_server/eap_methods.o
OBJS += ../src/eap_server/eap_identity.o

ifdef CONFIG_EAP
CFLAGS += -DEAP_SERVER
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifeq ($(CONFIG_TLS), internal)
ifndef CONFIG_CRYPTO
CONFIG_CRYPTO=internal
endif
endif
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
CFLAGS += -DCONFIG_INTERNAL_X509
endif
ifeq ($(CONFIG_CRYPTO), internal)
CFLAGS += -DCONFIG_INTERNAL_X509
endif


ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
CFLAGS += -DEAP_TLS_FUNCS
OBJS += ../src/eap_server/eap_tls_common.o
NEED_TLS_PRF=y
ifeq ($(CONFIG_TLS), openssl)
OBJS += ../src/crypto/tls_openssl.o
LIBS += -lssl -lcrypto
LIBS_p += -lcrypto
LIBS_h += -lcrypto
endif
ifeq ($(CONFIG_TLS), gnutls)
OBJS += ../src/crypto/tls_gnutls.o
LIBS += -lgnutls -lgcrypt -lgpg-error
LIBS_p += -lgcrypt
LIBS_h += -lgcrypt
endif
ifdef CONFIG_GNUTLS_EXTRA
CFLAGS += -DCONFIG_GNUTLS_EXTRA
LIBS += -lgnutls-extra
endif
ifeq ($(CONFIG_TLS), internal)
OBJS += ../src/crypto/tls_internal.o
OBJS += ../src/tls/tlsv1_common.o ../src/tls/tlsv1_record.o
OBJS += ../src/tls/tlsv1_cred.o ../src/tls/tlsv1_server.o
OBJS += ../src/tls/tlsv1_server_write.o ../src/tls/tlsv1_server_read.o
OBJS += ../src/tls/asn1.o ../src/tls/x509v3.o
OBJS_p += ../src/tls/asn1.o
OBJS_p += ../src/crypto/rc4.o ../src/crypto/aes_wrap.o ../src/crypto/aes.o
NEED_BASE64=y
CFLAGS += -DCONFIG_TLS_INTERNAL
CFLAGS += -DCONFIG_TLS_INTERNAL_SERVER
ifeq ($(CONFIG_CRYPTO), internal)
ifdef CONFIG_INTERNAL_LIBTOMMATH
CFLAGS += -DCONFIG_INTERNAL_LIBTOMMATH
else
LIBS += -ltommath
LIBS_p += -ltommath
endif
endif
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
LIBS += -ltomcrypt -ltfm
LIBS_p += -ltomcrypt -ltfm
endif
endif
NEED_CRYPTO=y
else
OBJS += ../src/crypto/tls_none.o
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef MS_FUNCS
OBJS += ../src/crypto/ms_funcs.o
NEED_CRYPTO=y
endif

ifdef CHAP
OBJS += ../src/eap_common/chap.o
endif

ifdef NEED_CRYPTO
ifndef TLS_FUNCS
ifeq ($(CONFIG_TLS), openssl)
LIBS += -lcrypto
LIBS_p += -lcrypto
LIBS_h += -lcrypto
endif
ifeq ($(CONFIG_TLS), gnutls)
LIBS += -lgcrypt
LIBS_p += -lgcrypt
LIBS_h += -lgcrypt
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
LIBS += -ltomcrypt -ltfm
LIBS_p += -ltomcrypt -ltfm
endif
endif
endif
ifeq ($(CONFIG_TLS), openssl)
OBJS += ../src/crypto/crypto_openssl.o
OBJS_p += ../src/crypto/crypto_openssl.o
HOBJS += ../src/crypto/crypto_openssl.o
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_TLS), gnutls)
OBJS += ../src/crypto/crypto_gnutls.o
OBJS_p += ../src/crypto/crypto_gnutls.o
HOBJS += ../src/crypto/crypto_gnutls.o
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
OBJS += ../src/crypto/crypto_libtomcrypt.o
OBJS_p += ../src/crypto/crypto_libtomcrypt.o
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_CRYPTO), internal)
OBJS += ../src/crypto/crypto_internal.o ../src/tls/rsa.o ../src/tls/bignum.o
OBJS_p += ../src/crypto/crypto_internal.o ../src/tls/rsa.o ../src/tls/bignum.o
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
endif
endif
else
CONFIG_INTERNAL_AES=y
CONFIG_INTERNAL_SHA1=y
CONFIG_INTERNAL_MD5=y
CONFIG_INTERNAL_SHA256=y
endif

ifdef CONFIG_INTERNAL_AES
CFLAGS += -DINTERNAL_AES
endif
ifdef CONFIG_INTERNAL_SHA1
CFLAGS += -DINTERNAL_SHA1
endif
ifdef CONFIG_INTERNAL_SHA256
CFLAGS += -DINTERNAL_SHA256
endif
ifdef CONFIG_INTERNAL_MD5
CFLAGS += -DINTERNAL_MD5
endif
ifdef CONFIG_INTERNAL_MD4
CFLAGS += -DINTERNAL_MD4
endif
ifdef CONFIG_INTERNAL_DES
CFLAGS += -DINTERNAL_DES
endif

ifdef NEED_SHA256
OBJS += ../src/crypto/sha256.o
endif

ifdef NEED_DH_GROUPS
OBJS += ../src/crypto/dh_groups.o
ifdef NEED_DH_GROUPS_ALL
CFLAGS += -DALL_DH_GROUPS
endif
endif

ifndef NEED_FIPS186_2_PRF
CFLAGS += -DCONFIG_NO_FIPS186_2_PRF
endif

ifndef NEED_T_PRF
CFLAGS += -DCONFIG_NO_T_PRF
endif

ifndef NEED_TLS_PRF
CFLAGS += -DCONFIG_NO_TLS_PRF
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

ifdef CONFIG_FULL_DYNAMIC_VLAN
# define CONFIG_FULL_DYNAMIC_VLAN to have hostapd manipulate bridges
# and vlan interfaces for the vlan feature.
CFLAGS += -DCONFIG_FULL_DYNAMIC_VLAN
endif

ifdef NEED_BASE64
OBJS += ../src/utils/base64.o
endif

ifdef CONFIG_NO_STDOUT_DEBUG
CFLAGS += -DCONFIG_NO_STDOUT_DEBUG
endif

ifdef CONFIG_NO_AES_EXTRAS
CFLAGS += -DCONFIG_NO_AES_UNWRAP
CFLAGS += -DCONFIG_NO_AES_CTR -DCONFIG_NO_AES_OMAC1
CFLAGS += -DCONFIG_NO_AES_EAX -DCONFIG_NO_AES_CBC
CFLAGS += -DCONFIG_NO_AES_DECRYPT
CFLAGS += -DCONFIG_NO_AES_ENCRYPT_BLOCK
endif

ALL=hostapd hostapd_cli

all: verify_config $(ALL)

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<

verify_config:
	@if [ ! -r .config ]; then \
		echo 'Building hostapd requires a configuration file'; \
		echo '(.config). See README for more instructions. You can'; \
		echo 'run "cp defconfig .config" to create an example'; \
		echo 'configuration.'; \
		exit 1; \
	fi

install: all
	for i in $(ALL); do cp $$i /usr/local/bin/$$i; done

hostapd: $(OBJS)
	$(CC) -o hostapd $(OBJS) $(LIBS)

OBJS_c = hostapd_cli.o ../src/common/wpa_ctrl.o ../src/utils/os_$(CONFIG_OS).o
hostapd_cli: $(OBJS_c)
	$(CC) -o hostapd_cli $(OBJS_c)

NOBJS = nt_password_hash.o ../src/crypto/ms_funcs.o ../src/crypto/sha1.o ../src/crypto/rc4.o ../src/crypto/md5.o
NOBJS += ../src/crypto/crypto_openssl.o ../src/utils/os_$(CONFIG_OS).o
ifdef TLS_FUNCS
LIBS_n += -lcrypto
endif

nt_password_hash: $(NOBJS)
	$(CC) -o nt_password_hash $(NOBJS) $(LIBS_n)

hlr_auc_gw: $(HOBJS)
	$(CC) -o hlr_auc_gw $(HOBJS) $(LIBS_h)

clean:
	$(MAKE) -C ../src clean
	rm -f core *~ *.o hostapd hostapd_cli nt_password_hash hlr_auc_gw
	rm -f *.d

%.eps: %.fig
	fig2dev -L eps $*.fig $*.eps

%.png: %.fig
	fig2dev -L png -m 3 $*.fig | pngtopnm | pnmscale 0.4 | pnmtopng \
		> $*.png

docs-pics: doc/hostapd.png doc/hostapd.eps

docs: docs-pics
	(cd ..; doxygen hostapd/doc/doxygen.full; cd hostapd)
	$(MAKE) -C doc/latex
	cp doc/latex/refman.pdf hostapd-devel.pdf

docs-fast: docs-pics
	(cd ..; doxygen hostapd/doc/doxygen.fast; cd hostapd)

clean-docs:
	rm -rf doc/latex doc/html
	rm -f doc/hostapd.{eps,png} hostapd-devel.pdf

TEST_SRC_MILENAGE = ../src/hlr_auc_gw/milenage.c ../src/crypto/aes_wrap.c ../src/crypto/aes.c ../src/utils/common.c ../src/utils/wpa_debug.o ../src/utils/os_$(CONFIG_OS).c
test-milenage: $(TEST_SRC_MILENAGE)
	$(CC) -o test-milenage -Wall -Werror $(TEST_SRC_MILENAGE) \
		-DTEST_MAIN_MILENAGE -I. -DINTERNAL_AES \
		-I../src/crypto -I../src/utils
	./test-milenage
	rm test-milenage

-include $(OBJS:%.o=%.d)
