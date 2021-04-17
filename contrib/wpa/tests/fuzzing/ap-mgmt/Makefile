ALL=ap-mgmt
include ../rules.include

CFLAGS += -DCONFIG_WNM
CFLAGS += -DCONFIG_INTERWORKING
CFLAGS += -DCONFIG_GAS
CFLAGS += -DCONFIG_HS20
CFLAGS += -DIEEE8021X_EAPOL
CFLAGS += -DNEED_AP_MLME
CFLAGS += -DCONFIG_AIRTIME_POLICY

LIBS += $(SRC)/common/libcommon.a
LIBS += $(SRC)/crypto/libcrypto.a
LIBS += $(SRC)/tls/libtls.a
LIBS += $(SRC)/wps/libwps.a
LIBS += $(SRC)/eap_server/libeap_server.a
LIBS += $(SRC)/eap_common/libeap_common.a
LIBS += $(SRC)/l2_packet/libl2_packet.a
LIBS += $(SRC)/ap/libap.a
LIBS += $(SRC)/eapol_auth/libeapol_auth.a
LIBS += $(SRC)/radius/libradius.a
LIBS += $(SRC)/utils/libutils.a

ELIBS += $(SRC)/crypto/libcrypto.a
ELIBS += $(SRC)/tls/libtls.a

OBJS += $(SRC)/drivers/driver_common.o

OBJS += ap-mgmt.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

_OBJS_VAR := ELIBS
include ../../../src/objs.mk

ap-mgmt: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS) $(ELIBS)

clean: common-clean
	rm -f ap-mgmt *~ *.o *.d ../*~ ../*.o ../*.d
