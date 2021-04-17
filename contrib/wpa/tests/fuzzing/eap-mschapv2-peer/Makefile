ALL=eap-mschapv2-peer
include ../rules.include

CFLAGS += -DIEEE8021X_EAPOL

OBJS += $(SRC)/eap_peer/eap_mschapv2.o
OBJS += $(SRC)/eap_peer/mschapv2.o
OBJS += $(SRC)/eap_common/eap_common.o
LIBS += $(SRC)/crypto/libcrypto.a
LIBS += $(SRC)/utils/libutils.a

OBJS += eap-mschapv2-peer.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

eap-mschapv2-peer: $(OBJS) $(LIBS)
	$(Q)$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS) $(ELIBS)
	@$(E) "  LD " $@

clean: common-clean
	rm -f eap-mschapv2-peer *~ *.o *.d ../*~ ../*.o ../*.d
