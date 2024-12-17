ROOT_DIR=../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

# Basic examples directory
BASIC_DIR=basic/
# Hash functions directory
HASH_DIR=hash/
# Signature functions directory
SIG_DIR=sig/
# Shamir Secret Sharing directory
SSS_DIR=sss/

CFLAGS += -I$(ROOT_DIR)/include/

all:	_basic _hash _sig _sss
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	_basic_dync _hash_dyn _sig_dyn _sss_dyn
endif

_basic:
	$(VERBOSE_MAKE)cd $(BASIC_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_hash:
	$(VERBOSE_MAKE)cd $(HASH_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_sig:
	$(VERBOSE_MAKE)cd $(SIG_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_sss:
	$(VERBOSE_MAKE)cd $(SSS_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
_basic_dync:
	$(VERBOSE_MAKE)cd $(BASIC_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_hash_dyn:
	$(VERBOSE_MAKE)cd $(HASH_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_sig_dyn:
	$(VERBOSE_MAKE)cd $(SIG_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_sss_dyn:
	$(VERBOSE_MAKE)cd $(SSS_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
endif


clean:
	$(VERBOSE_MAKE)cd $(BASIC_DIR) && make clean
	$(VERBOSE_MAKE)cd $(HASH_DIR) && make clean
	$(VERBOSE_MAKE)cd $(SIG_DIR) && make clean
	$(VERBOSE_MAKE)cd $(SSS_DIR) && make clean

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
