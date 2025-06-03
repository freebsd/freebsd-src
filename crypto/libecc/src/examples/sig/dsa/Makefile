ROOT_DIR=../../../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

# Remove old includes with bad relative folders
CFLAGS := $(patsubst -I%,,$(CFLAGS))
# Add proper includes
CFLAGS += -I$(ROOT_DIR)/include/

all:	dsa
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	dsa_dyn
endif

libhash:
	# Compile the hashes
	$(VERBOSE_MAKE)cd ../../hash && EXTRA_CFLAGS="$(CFLAGS)" make libhash.a

dsa: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DDSA dsa.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) ../../hash/libhash.a $(LIBSIGN) -o dsa

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
dsa_dyn: libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DDSA dsa.c ../../hash/hash.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) $(LIBEC) -L$(BUILD_DIR) ../../hash/libhash.a -lsign -o dsa_dyn
endif


clean:
	@rm -f dsa
	@rm -f dsa_dyn

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
