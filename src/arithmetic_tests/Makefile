ROOT_DIR=../..
BUILD_DIR=$(ROOT_DIR)/build
# NOTE: we do not want warnings to pollute our compilation ...
WNOERROR=1
include $(ROOT_DIR)/common.mk

BIN_CFLAGS += -I$(ROOT_DIR)/include/ -Wno-shadow -Wno-conversion -Wno-cast-align -Wno-unused-macros

# Remove some false alarms
BIN_CFLAGS += -Wno-unused-but-set-variable -Wno-declaration-after-statement

NUM_TESTS ?= 1000

all: bin tests

bin:
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) -DDEBUG -DWITH_STDLIB arithmetic_tests.c $(LIBARITH) $(ROOT_DIR)/src/external_deps/print.c $(ROOT_DIR)/src/external_deps/rand.c $(BIN_LDFLAGS) -o arithmetic_tests

tests:	bin
	# Get the word size and maximum nn bit length
	$(eval WORDSZ := $(shell ./arithmetic_tests -info | cut -d ' ' -f1))
	$(eval MAXBITS := $(shell ./arithmetic_tests -info | cut -d ' ' -f2))
	python2 arithmetic_tests_generator.py stdout $(WORDSZ) $(MAXBITS) $(NUM_TESTS) | ./arithmetic_tests

clean:
	@rm -f arithmetic_tests
