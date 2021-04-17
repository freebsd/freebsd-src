ALL=sae
include ../rules.include

CFLAGS += -DCONFIG_SHA256
CFLAGS += -DCONFIG_ECC

LIBS += $(SRC)/common/libcommon.a
LIBS += $(SRC)/utils/libutils.a

OBJS += $(SRC)/crypto/crypto_openssl.o
OBJS += $(SRC)/crypto/dh_groups.o
OBJS += $(SRC)/crypto/sha256-prf.o
OBJS += $(SRC)/crypto/sha256-kdf.o
OBJS += $(SRC)/common/dragonfly.o

OBJS += sae.o

_OBJS_VAR := OBJS
include ../../../src/objs.mk

_OBJS_VAR := LIBS
include ../../../src/objs.mk

sae: $(OBJS) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ -lcrypto

clean: common-clean
	rm -f sae *~ *.o *.d ../*~ ../*.o ../*.d
