ALL=asn1
include ../rules.include

OBJS += $(SRC)/utils/common.o
OBJS += $(SRC)/utils/os_unix.o
OBJS += $(SRC)/utils/wpa_debug.o
OBJS += $(SRC)/utils/wpabuf.o
OBJS += $(SRC)/tls/asn1.o

OBJS += asn1.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

asn1: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS) $(ELIBS)

clean: common-clean
	$(MAKE) -C $(SRC) clean
	rm -f asn1 *~ *.o *.d ../*~ ../*.o ../*.d
