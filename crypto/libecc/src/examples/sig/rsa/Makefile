ROOT_DIR=../../../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

# Remove old includes with bad relative folders
CFLAGS := $(patsubst -I%,,$(CFLAGS))
# Add proper includes
CFLAGS += -I$(ROOT_DIR)/include/

all:	rsa
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	rsa_dyn
endif

libhash:
	# Compile the hashes
	$(VERBOSE_MAKE)cd ../../hash && EXTRA_CFLAGS="$(CFLAGS)" make libhash.a

rsa: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DRSA rsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) ../../hash/libhash.a $(LIBSIGN) -o rsa

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
rsa_dyn: libhash
	# Compile the hashes
	$(VERBOSE_MAKE)cd ../../hash && EXTRA_CFLAGS="$(CFLAGS)" make _li
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DRSA rsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) $(LIBEC) -L$(BUILD_DIR) ../../hash/libhash.a -lsign -o rsa_dyn
endif


clean:
	@rm -f rsa
	@rm -f rsa_dyn

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
