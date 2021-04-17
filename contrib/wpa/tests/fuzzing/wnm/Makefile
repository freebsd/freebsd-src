ALL=wnm
include ../rules.include

CFLAGS += -DCONFIG_WNM
CFLAGS += -DCONFIG_INTERWORKING
CFLAGS += -DCONFIG_GAS
CFLAGS += -DCONFIG_HS20
CFLAGS += -DIEEE8021X_EAPOL

LIBS += $(SRC)/common/libcommon.a
LIBS += $(SRC)/crypto/libcrypto.a
LIBS += $(SRC)/tls/libtls.a
LIBS += $(SRC)/rsn_supp/librsn_supp.a
LIBS += $(SRC)/eapol_supp/libeapol_supp.a
LIBS += $(SRC)/eap_peer/libeap_peer.a
LIBS += $(SRC)/eap_common/libeap_common.a
LIBS += $(SRC)/l2_packet/libl2_packet.a
LIBS += $(SRC)/utils/libutils.a

ELIBS += $(SRC)/crypto/libcrypto.a
ELIBS += $(SRC)/tls/libtls.a

OBJS += $(WPAS_SRC)/bssid_ignore.o
OBJS += $(WPAS_SRC)/bss.o
OBJS += $(WPAS_SRC)/config.o
OBJS += $(WPAS_SRC)/config_file.o
OBJS += $(WPAS_SRC)/eap_register.o
OBJS += $(WPAS_SRC)/events.o
OBJS += $(WPAS_SRC)/gas_query.o
OBJS += $(WPAS_SRC)/hs20_supplicant.o
OBJS += $(WPAS_SRC)/interworking.o
OBJS += $(WPAS_SRC)/notify.o
OBJS += $(WPAS_SRC)/offchannel.o
OBJS += $(WPAS_SRC)/op_classes.o
OBJS += $(WPAS_SRC)/robust_av.o
OBJS += $(WPAS_SRC)/rrm.o
OBJS += $(WPAS_SRC)/scan.o
OBJS += $(WPAS_SRC)/wmm_ac.o
OBJS += $(WPAS_SRC)/wnm_sta.o
OBJS += $(WPAS_SRC)/wpa_supplicant.o
OBJS += $(WPAS_SRC)/wpas_glue.o
OBJS += $(SRC)/drivers/driver_common.o
OBJS += $(SRC)/drivers/drivers.o

OBJS += wnm.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

_OBJS_VAR := ELIBS
include ../../../src/objs.mk

wnm: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS) $(ELIBS)

clean: common-clean
	rm -f wnm *~ *.o *.d ../*~ ../*.o ../*.d
