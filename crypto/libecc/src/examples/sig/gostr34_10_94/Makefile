ROOT_DIR=../../../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

# Remove old includes with bad relative folders
CFLAGS := $(patsubst -I%,,$(CFLAGS))
# Add proper includes
CFLAGS += -I$(ROOT_DIR)/include/

all:	gostr34_10_94
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	gostr34_10_94_dyn
endif

libhash:
	# Compile the hashes
	$(VERBOSE_MAKE)cd ../../hash && EXTRA_CFLAGS="$(CFLAGS)" make libhash.a

gostr34_10_94: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DGOSTR34_10_94 gostr34_10_94.c ../dsa/dsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) ../../hash/libhash.a $(LIBSIGN) -o gostr34_10_94

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
gostr34_10_94_dyn: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DGOSTR34_10_94 gostr34_10_94.c ../dsa/dsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) ../../hash/libhash.a $(LIBEC) -L$(BUILD_DIR) -lsign -o gostr34_10_94_dyn
endif


clean:
	@rm -f gostr34_10_94
	@rm -f gostr34_10_94_dyn

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
