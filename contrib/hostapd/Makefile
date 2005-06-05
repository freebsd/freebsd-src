CC=gcc
DIR_WPA_SUPPLICANT=.
DIR_HOSTAP=.

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

# define HOSTAPD_DUMP_STATE to include SIGUSR1 handler for dumping state to
# a file (undefine it, if you want to save in binary size)
CFLAGS += -DHOSTAPD_DUMP_STATE

# Include directories for CVS version
CFLAGS += -I. -I$(DIR_HOSTAP) -I../utils -I$(DIR_WPA_SUPPLICANT)

# Uncomment following line and set the path to your kernel tree include
# directory if your C library does not include all header files.
# CFLAGS += -DUSE_KERNEL_HEADERS -I/usr/src/linux/include

OBJS =	hostapd.o eloop.o ieee802_1x.o eapol_sm.o radius.o md5.o rc4.o \
	common.o ieee802_11.o config.o ieee802_11_auth.o accounting.o \
	sta_info.o radius_client.o sha1.o wpa.o aes_wrap.o ctrl_iface.o \
	driver_conf.o

-include .config

ifdef CONFIG_IAPP
CFLAGS += -DCONFIG_IAPP
OBJS += iapp.o
endif

ifdef CONFIG_RSN_PREAUTH
CFLAGS += -DCONFIG_RSN_PREAUTH
CONFIG_L2_PACKET=y
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

ifdef CONFIG_DRIVER_BSD
CFLAGS += -DCONFIG_DRIVER_BSD
OBJS += driver_bsd.o
CONFIG_L2_PACKET=y
CONFIG_DNET_PCAP=y
endif

ifdef CONFIG_DRIVER_TEST
CFLAGS += -DCONFIG_DRIVER_TEST
OBJS += driver_test.o
endif

ifdef CONFIG_L2_PACKET
OBJS += $(DIR_WPA_SUPPLICANT)/l2_packet.o
endif

ifdef CONFIG_DNET_PCAP
CFLAGS += -DUSE_DNET_PCAP
LIBS +=-ldnet -lpcap
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
OBJS += eap_sim.o $(DIR_WPA_SUPPLICANT)/eap_sim_common.o
# Example EAP-SIM interface for GSM authentication. This can be replaced with
# another file implementating the interface specified in eap_sim_db.h.
OBJS += eap_sim_db.o
endif

ifdef CONFIG_EAP_TLV
CFLAGS += -DEAP_TLV
OBJS += eap_tlv.o
endif

ifdef CONFIG_EAP
CFLAGS += -DEAP_AUTHENTICATOR
OBJS += eap.o eap_identity.o
endif

ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, and EAP_TTLS)
CFLAGS += -DEAP_TLS_FUNCS
OBJS += eap_tls_common.o $(DIR_WPA_SUPPLICANT)/tls_openssl.o
LIBS += -lssl -lcrypto
LIBS_p += -lcrypto
else
OBJS += $(DIR_WPA_SUPPLICANT)/tls_none.o
endif

ifdef CONFIG_PKCS12
CFLAGS += -DPKCS12_FUNCS
endif

ifdef MS_FUNCS
ifndef TLS_FUNCS
LIBS += -lcrypto
endif
OBJS += $(DIR_WPA_SUPPLICANT)/ms_funcs.o $(DIR_WPA_SUPPLICANT)/crypto.o
endif

ifdef CONFIG_RADIUS_SERVER
CFLAGS += -DRADIUS_SERVER
OBJS += radius_server.o
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
	echo '#include <stdlib.h>'			>> driver_conf.c
	echo '#include <stdio.h>'			>> driver_conf.c
	echo '#include <sys/types.h>'			>> driver_conf.c
	echo '#include <netinet/in.h>'			>> driver_conf.c
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
ifdef CONFIG_DRIVER_BSD
	echo "bsd_driver_register();"			>> driver_conf.c
endif
ifdef CONFIG_DRIVER_TEST
	echo "test_driver_register();"			>> driver_conf.c
endif
	echo '}'					>> driver_conf.c

hostapd_cli: hostapd_cli.o hostapd_ctrl.o
	$(CC) -o hostapd_cli hostapd_cli.o hostapd_ctrl.o

clean:
	rm -f core *~ *.o hostapd *.d driver_conf.c

-include $(OBJS:%.o=%.d)
