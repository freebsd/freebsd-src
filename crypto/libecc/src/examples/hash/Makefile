ROOT_DIR=../../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

# Remove old includes with bad relative folders
CFLAGS := $(patsubst -I%,,$(CFLAGS))
# Add proper includes
CFLAGS += -I$(ROOT_DIR)/include/

HASHES = gostr34_11_94.c md2.c md4.c md5.c mdc2.c sha0.c sha1.c tdes.c

all:	libhash.a _hash
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	libhash.a _hash_dyn
endif

libhash.a: $(HASHES) hash.c
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(LIB_CFLAGS) $(HASHES) hash.c -c
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(AR) $(AR_FLAGS) libhash.a *.o
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(RANLIB) $(RANLIB_FLAGS) libhash.a

_hash:
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DHASH $(HASHES) hash.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) $(LIBSIGN) -o hash

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
_hash_dyn: _libhash
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DHASH $(HASHES) hash.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) $(LIBEC) -L$(BUILD_DIR) -lsign -o hash_dyn
endif


clean:
	@rm -f *.o libhash.a hash
	@rm -f *.o libhash.a hash_dyn

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
