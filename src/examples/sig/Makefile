ROOT_DIR=../../../
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

RSA_DIR=rsa/
DSA_DIR=dsa/
SDSA_DIR=sdsa/
KCDSA_DIR=kcdsa/
GOST_DIR=gostr34_10_94/

CFLAGS += -I$(ROOT_DIR)/include/

all:	_rsa _dsa _sdsa _kcdsa _gost
ifeq ($(WITH_DYNAMIC_LIBS),1)
# If the user asked for dynamic libraries, compile versions of our binaries against them
all:	_rsa_dyn _dsa_dyn _sdsa_dyn _kcdsa_dyn _gost_dyn
endif

_rsa:
	$(VERBOSE_MAKE)cd $(RSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_dsa:
	$(VERBOSE_MAKE)cd $(DSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_sdsa:
	$(VERBOSE_MAKE)cd $(SDSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_kcdsa:
	$(VERBOSE_MAKE)cd $(KCDSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_gost:
	$(VERBOSE_MAKE)cd $(GOST_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make

# If the user asked for dynamic libraries, compile versions of our binaries against them
ifeq ($(WITH_DYNAMIC_LIBS),1)
_rsa_dyn:
	$(VERBOSE_MAKE)cd $(RSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_dsa_dyn:
	$(VERBOSE_MAKE)cd $(DSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_sdsa_dyn:
	$(VERBOSE_MAKE)cd $(SDSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_kcdsa_dyn:
	$(VERBOSE_MAKE)cd $(KCDSA_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
_gost_dyn:
	$(VERBOSE_MAKE)cd $(GOST_DIR) && EXTRA_CFLAGS="$(CFLAGS)" make
endif


clean:
	$(VERBOSE_MAKE)cd $(RSA_DIR) && make clean
	$(VERBOSE_MAKE)cd $(DSA_DIR) && make clean
	$(VERBOSE_MAKE)cd $(SDSA_DIR) && make clean
	$(VERBOSE_MAKE)cd $(KCDSA_DIR) && make clean
	$(VERBOSE_MAKE)cd $(GOST_DIR) && make clean

.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64
