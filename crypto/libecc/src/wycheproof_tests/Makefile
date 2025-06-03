ROOT_DIR=../../
ROOT_DIR=../..
BUILD_DIR=$(ROOT_DIR)/build
include $(ROOT_DIR)/common.mk

ifeq ($(VERBOSE_ACCEPTABLE), 1)
CFLAGS += -DVERBOSE_ACCEPTABLE
endif

CFLAGS += -I$(ROOT_DIR)/include/

all:
	$(CC) $(BIN_CFLAGS) libecc_wycheproof.c $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(LIBSIGN) $(BIN_LDFLAGS) -o libecc_wycheproof

clean:
	@rm -f libecc_wycheproof

