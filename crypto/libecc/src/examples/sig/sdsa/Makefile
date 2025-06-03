ROOT_DIR=../../../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

# Remove old includes with bad relative folders
CFLAGS := $(patsubst -I%,,$(CFLAGS))
# Add proper includes
CFLAGS += -I$(ROOT_DIR)/include/

all:	sdsa
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	sdsa_dyn
endif

libhash:
	# Compile the hashes
	$(VERBOSE_MAKE)cd ../../hash && EXTRA_CFLAGS="$(CFLAGS)" make libhash.a

sdsa: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DSDSA sdsa.c ../../hash/hash.c ../dsa/dsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) ../../hash/libhash.a $(LIBSIGN) -o sdsa

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
sdsa_dyn: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DSDSA sdsa.c ../../hash/hash.c ../dsa/dsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) $(LIBEC) -L$(BUILD_DIR) ../../hash/libhash.a -lsign -o sdsa_dyn
endif


clean:
	@rm -f sdsa
	@rm -f sdsa_dyn

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
