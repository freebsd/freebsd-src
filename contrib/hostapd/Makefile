CC=gcc
DIR_WPA_SUPPLICANT=.

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
CFLAGS += -DHOSTAPD_DUMP_STATE

# Include directories for CVS version
CFLAGS += -I. -I../utils -I$(DIR_WPA_SUPPLICANT)

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

OBJS =	hostapd.o eloop.o ieee802_1x.o eapol_sm.o radius.o md5.o rc4.o md4.o \
	common.o ieee802_11.o config.o ieee802_11_auth.o accounting.o \
	sta_info.o radius_client.o sha1.o wpa.o aes_wrap.o ctrl_iface.o \
	driver_conf.o os_$(CONFIG_OS).o preauth.o pmksa_cache.o beacon.o \
	hw_features.o wme.o ap_list.o reconfig.o \
	mlme.o vlan_init.o ieee802_11h.o

HOBJS=hlr_auc_gw.o common.o os_$(CONFIG_OS).o milenage.o aes_wrap.o

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
endif

ifdef CONFIG_IEEE80211W
CFLAGS += -DCONFIG_IEEE80211W
NEED_SHA256=y
endif

ifdef CONFIG_DRIVER_HOSTAP
CFLAGS += -DCONFIG_DRIVER_HOSTAP
OBJS += driver.o
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

ifdef CONFIG_DRIVER_PRISM54
CFLAGS += -DCONFIG_DRIVER_PRISM54
OBJS += driver_prism54.o
endif

ifdef CONFIG_DRIVER_DEVICESCAPE
CFLAGS += -DCONFIG_DRIVER_DEVICESCAPE
OBJS += driver_devicescape.o
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

ifdef CONFIG_L2_PACKET
ifdef CONFIG_DNET_PCAP
ifdef CONFIG_L2_FREEBSD
LIBS += -lpcap
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet_freebsd.o
else
LIBS += -ldnet -lpcap
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet_pcap.o
endif
else
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet_linux.o
endif
endif


ifdef CONFIG_EAP_MD5
CFLAGS += -DEAP_MD5
OBJS += eap_md5.o
endif

ifdef CONFIG_EAP_TLS
CFLAGS += -DEAP_TLS
OBJS += eap_tls.o
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_PEAP
CFLAGS += -DEAP_PEAP
OBJS += eap_peap.o
TLS_FUNCS=y
CONFIG_EAP_TLV=y
CONFIG_EAP_MSCHAPV2=y
endif

ifdef CONFIG_EAP_TTLS
CFLAGS += -DEAP_TTLS
OBJS += eap_ttls.o
TLS_FUNCS=y
endif

ifdef CONFIG_EAP_MSCHAPV2
CFLAGS += -DEAP_MSCHAPv2
OBJS += eap_mschapv2.o
MS_FUNCS=y
endif

ifdef CONFIG_EAP_GTC
CFLAGS += -DEAP_GTC
OBJS += eap_gtc.o
endif

ifdef CONFIG_EAP_SIM
CFLAGS += -DEAP_SIM
OBJS += eap_sim.o
CONFIG_EAP_SIM_COMMON=y
endif

ifdef CONFIG_EAP_AKA
CFLAGS += -DEAP_AKA
OBJS += eap_aka.o
CONFIG_EAP_SIM_COMMON=y
endif

ifdef CONFIG_EAP_SIM_COMMON
OBJS += $(DIR_WPA_SUPPLICANT)/eap_sim_common.o
# Example EAP-SIM/AKA interface for GSM/UMTS authentication. This can be
# replaced with another file implementating the interface specified in
# eap_sim_db.h.
OBJS += eap_sim_db.o
endif

ifdef CONFIG_EAP_PAX
CFLAGS += -DEAP_PAX
OBJS += eap_pax.o $(DIR_WPA_SUPPLICANT)/eap_pax_common.o
endif

ifdef CONFIG_EAP_PSK
CFLAGS += -DEAP_PSK
OBJS += eap_psk.o $(DIR_WPA_SUPPLICANT)/eap_psk_common.o
endif

ifdef CONFIG_EAP_SAKE
CFLAGS += -DEAP_SAKE
OBJS += eap_sake.o $(DIR_WPA_SUPPLICANT)/eap_sake_common.o
endif

ifdef CONFIG_EAP_GPSK
CFLAGS += -DEAP_GPSK
OBJS += eap_gpsk.o $(DIR_WPA_SUPPLICANT)/eap_gpsk_common.o
ifdef CONFIG_EAP_GPSK_SHA256
CFLAGS += -DEAP_GPSK_SHA256
NEED_SHA256=y
endif
endif

ifdef CONFIG_EAP_VENDOR_TEST
CFLAGS += -DEAP_VENDOR_TEST
OBJS += eap_vendor_test.o
endif

ifdef CONFIG_EAP_TLV
CFLAGS += -DEAP_TLV
OBJS += eap_tlv.o
endif

ifdef CONFIG_EAP
CFLAGS += -DEAP_SERVER
OBJS += eap.o eap_methods.o eap_identity.o
endif

ifndef CONFIG_TLS
CONFIG_TLS=openssl
endif

ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
CFLAGS += -DEAP_TLS_FUNCS
OBJS += eap_tls_common.o
ifeq ($(CONFIG_TLS), openssl)
OBJS += $(DIR_WPA_SUPPLICANT)/tls_openssl.o
LIBS += -lssl -lcrypto
LIBS_p += -lcrypto
LIBS_h += -lcrypto
endif
ifeq ($(CONFIG_TLS), gnutls)
OBJS += $(DIR_WPA_SUPPLICANT)/tls_gnutls.o
LIBS += -lgnutls -lgcrypt -lgpg-error
LIBS_p += -lgcrypt
LIBS_h += -lgcrypt
endif
ifdef CONFIG_GNUTLS_EXTRA
CFLAGS += -DCONFIG_GNUTLS_EXTRA
LIBS += -lgnutls-extra
endif
NEED_CRYPTO=y
else
OBJS += $(DIR_WPA_SUPPLICANT)/tls_none.o
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef MS_FUNCS
OBJS += $(DIR_WPA_SUPPLICANT)/ms_funcs.o
NEED_CRYPTO=y
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
endif
ifeq ($(CONFIG_TLS), openssl)
OBJS += $(DIR_WPA_SUPPLICANT)/crypto.o
OBJS_p += $(DIR_WPA_SUPPLICANT)/crypto.o
HOBJS += $(DIR_WPA_SUPPLICANT)/crypto.o
CONFIG_INTERNAL_SHA256=y
endif
ifeq ($(CONFIG_TLS), gnutls)
OBJS += $(DIR_WPA_SUPPLICANT)/crypto_gnutls.o
OBJS_p += $(DIR_WPA_SUPPLICANT)/crypto_gnutls.o
HOBJS += $(DIR_WPA_SUPPLICANT)/crypto_gnutls.o
CONFIG_INTERNAL_SHA256=y
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

ifdef NEED_SHA256
OBJS += sha256.o
endif

ifdef CONFIG_RADIUS_SERVER
CFLAGS += -DRADIUS_SERVER
OBJS += radius_server.o
endif

ifdef CONFIG_IPV6
CFLAGS += -DCONFIG_IPV6
endif

ifdef CONFIG_FULL_DYNAMIC_VLAN
# define CONFIG_FULL_DYNAMIC_VLAN to have hostapd manipulate bridges
# and vlan interfaces for the vlan feature.
CFLAGS += -DCONFIG_FULL_DYNAMIC_VLAN
endif

ALL=hostapd hostapd_cli

all: verify_config $(ALL)

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

driver_conf.c: Makefile .config
	rm -f driver_conf.c
	echo '/* THIS FILE AUTOMATICALLY GENERATED, DO NOT EDIT! */' \
							> driver_conf.c
	echo '#include "includes.h"'			>> driver_conf.c
	echo '#include "hostapd.h"'			>> driver_conf.c
	echo '#include "driver.h"'			>> driver_conf.c
ifdef CONFIG_DRIVER_HOSTAP
	echo "void hostap_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_WIRED
	echo "void wired_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_MADWIFI
	echo "void madwifi_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_PRISM54
	echo "void prism54_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_DEVICESCAPE
	echo "void devicescape_driver_register(void);"	>> driver_conf.c
endif
ifdef CONFIG_DRIVER_BSD
	echo "void bsd_driver_register(void);"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "void test_driver_register(void);"		>> driver_conf.c
endif
	echo 'void register_drivers(void) {'		>> driver_conf.c
ifdef CONFIG_DRIVER_HOSTAP
	echo "hostap_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_WIRED
	echo "wired_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_MADWIFI
	echo "madwifi_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_PRISM54
	echo "prism54_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_DEVICESCAPE
	echo "devicescape_driver_register();"		>> driver_conf.c
endif
ifdef CONFIG_DRIVER_BSD
	echo "bsd_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "test_driver_register();"			>> driver_conf.c
endif
	echo '}'					>> driver_conf.c

hostapd_cli: hostapd_cli.o $(DIR_WPA_SUPPLICANT)/wpa_ctrl.o
	$(CC) -o hostapd_cli hostapd_cli.o $(DIR_WPA_SUPPLICANT)/wpa_ctrl.o

NOBJS = nt_password_hash.o $(DIR_WPA_SUPPLICANT)/ms_funcs.o sha1.o rc4.o md5.o
NOBJS += $(DIR_WPA_SUPPLICANT)/crypto.o os_$(CONFIG_OS).o
ifdef TLS_FUNCS
LIBS_n += -lcrypto
endif

nt_password_hash: $(NOBJS)
	$(CC) -o nt_password_hash $(NOBJS) $(LIBS_n)

hlr_auc_gw: $(HOBJS)
	$(CC) -o hlr_auc_gw $(HOBJS) $(LIBS_h)

clean:
	rm -f core *~ *.o hostapd hostapd_cli nt_password_hash hlr_auc_gw
	rm -f *.d driver_conf.c

%.eps: %.fig
	fig2dev -L eps $*.fig $*.eps

%.png: %.fig
	fig2dev -L png -m 3 $*.fig | pngtopnm | pnmscale 0.4 | pnmtopng \
		> $*.png

docs-pics: doc/hostapd.png doc/hostapd.eps

docs: docs-pics
	doxygen doc/doxygen.full
	$(MAKE) -C doc/latex
	cp doc/latex/refman.pdf hostapd-devel.pdf

docs-fast: docs-pics
	doxygen doc/doxygen.fast

clean-docs:
	rm -rf doc/latex doc/html
	rm -f doc/hosta.d{eps,png} hostapd-devel.pdf

TEST_SRC_MILENAGE = milenage.c aes_wrap.c common.c os_$(CONFIG_OS).c
test-milenage: $(TEST_SRC_MILENAGE)
	$(CC) -o test-milenage -Wall -Werror $(TEST_SRC_MILENAGE) \
		-DTEST_MAIN_MILENAGE -I. -I../wpa_supplicant -DINTERNAL_AES
	./test-milenage
	rm test-milenage

-include $(OBJS:%.o=%.d)
