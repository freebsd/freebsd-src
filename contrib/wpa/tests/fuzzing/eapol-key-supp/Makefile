ALL=eapol-key-supp
include ../rules.include

CFLAGS += -DCONFIG_IEEE80211R_AP
CFLAGS += -DCONFIG_IEEE80211R
CFLAGS += -DCONFIG_TDLS

LIBS += $(SRC)/common/libcommon.a
LIBS += $(SRC)/crypto/libcrypto.a
LIBS += $(SRC)/tls/libtls.a
LIBS += $(SRC)/rsn_supp/librsn_supp.a
LIBS += $(SRC)/eapol_supp/libeapol_supp.a
LIBS += $(SRC)/eap_peer/libeap_peer.a
LIBS += $(SRC)/eap_common/libeap_common.a
LIBS += $(SRC)/l2_packet/libl2_packet.a
LIBS += $(SRC)/utils/libutils.a

OBJS += eapol-key-supp.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

eapol-key-supp: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ -Wl,--start-group $(LIBS) -Wl,--end-group

clean: common-clean
	rm -f eapol-key-supp *~ *.o *.d ../*~ ../*.o ../*.d
