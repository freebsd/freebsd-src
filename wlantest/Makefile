ALL=wlantest wlantest_cli test_vectors

include ../src/build.rules

UNAME := $(shell uname -s)

CFLAGS += -I.
CFLAGS += -I../src
CFLAGS += -I../src/utils

ifneq ($(UNAME),Darwin)
# glibc < 2.17 needs -lrt for clock_gettime()
LIBS += -lrt
endif

OWN_LIBS += ../src/utils/libutils.a
OWN_LIBS += ../src/crypto/libcrypto.a

CFLAGS += -DCONFIG_OCV
CFLAGS += -DCONFIG_IEEE80211R
CFLAGS += -DCONFIG_HS20
CFLAGS += -DCONFIG_DEBUG_FILE
CFLAGS += -DCONFIG_FILS
CFLAGS += -DCONFIG_SAE
CFLAGS += -DCONFIG_OWE
CFLAGS += -DCONFIG_DPP
CFLAGS += -DCONFIG_SHA384
CFLAGS += -DCONFIG_PASN

OBJS += ../src/common/ieee802_11_common.o
OBJS += ../src/common/wpa_common.o
OBJS += ../src/radius/radius.o
OBJS += ../src/rsn_supp/wpa_ie.o

OBJS += wlantest.o
OBJS += readpcap.o
OBJS += writepcap.o
OBJS += monitor.o
OBJS += process.o
OBJS += wired.o
OBJS += rx_mgmt.o
OBJS += rx_data.o
OBJS += rx_eapol.o
OBJS += rx_ip.o
OBJS += rx_tdls.o
OBJS += bss.o
OBJS += sta.o
OBJS += ccmp.o
OBJS += tkip.o
OBJS += ctrl.o
OBJS += inject.o
OBJS += wep.o
OBJS += bip.o
OBJS += gcmp.o

LIBS += -lpcap

TOBJS += test_vectors.o
TOBJS += ccmp.o
TOBJS += tkip.o
TOBJS += wep.o
TOBJS += bip.o
TOBJS += gcmp.o


OBJS_cli = wlantest_cli.o

_OBJS_VAR := OBJS
include ../src/objs.mk
_OBJS_VAR := TOBJS
include ../src/objs.mk
_OBJS_VAR := OBJS_cli
include ../src/objs.mk
_OBJS_VAR := OWN_LIBS
include ../src/objs.mk

wlantest: $(OBJS) $(OWN_LIBS)
	$(LDO) $(LDFLAGS) -o wlantest $(OBJS) $(OWN_LIBS) $(LIBS)

wlantest_cli: $(OBJS_cli) $(OWN_LIBS)
	$(LDO) $(LDFLAGS) -o wlantest_cli $(OBJS_cli) $(OWN_LIBS) $(LIBS)

test_vectors: $(TOBJS) $(OWN_LIBS)
	$(LDO) $(LDFLAGS) -o test_vectors $(TOBJS) $(OWN_LIBS) $(LIBS)

clean: common-clean
	rm -f core *~
