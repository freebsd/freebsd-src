ALL=p2p
include ../rules.include

LIBS += $(SRC)/utils/libutils.a
LIBS += $(SRC)/common/libcommon.a
LIBS += $(SRC)/crypto/libcrypto.a
LIBS += $(SRC)/p2p/libp2p.a
LIBS += $(SRC)/tls/libtls.a
LIBS += $(SRC)/wps/libwps.a

OBJS += p2p.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

p2p: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LIBS)

clean: common-clean
	rm -f p2p *~ *.o *.d ../*~ ../*.o ../*.d
