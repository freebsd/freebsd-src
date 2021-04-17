ALL=test-base64 test-md4 test-milenage \
	test-rsa-sig-ver \
	test-sha1 \
	test-https test-https_server \
	test-sha256 test-aes test-x509v3 test-list test-rc4

include ../src/build.rules

ifdef LIBFUZZER
CC=clang
CFLAGS = -MMD -O2 -Wall -g
CFLAGS += -fsanitize=fuzzer,address,signed-integer-overflow,unsigned-integer-overflow
CFLAGS += -DTEST_LIBFUZZER
LDFLAGS += -fsanitize=fuzzer,address,signed-integer-overflow,unsigned-integer-overflow
TEST_FUZZ=y
endif

ifdef TEST_FUZZ
CFLAGS += -DCONFIG_NO_RANDOM_POOL
CFLAGS += -DTEST_FUZZ
endif

CFLAGS += -DCONFIG_IEEE80211R_AP
CFLAGS += -DCONFIG_IEEE80211R
CFLAGS += -DCONFIG_TDLS

CFLAGS += -I../src
CFLAGS += -I../src/utils

SLIBS = ../src/utils/libutils.a

DLIBS = ../src/crypto/libcrypto.a \
	../src/tls/libtls.a

_OBJS_VAR := LLIBS
include ../src/objs.mk
_OBJS_VAR := SLIBS
include ../src/objs.mk
_OBJS_VAR := DLIBS
include ../src/objs.mk

LIBS = $(SLIBS) $(DLIBS)
LLIBS = -Wl,--start-group $(DLIBS) -Wl,--end-group $(SLIBS)

# glibc < 2.17 needs -lrt for clock_gettime()
LLIBS += -lrt

test-aes: $(call BUILDOBJ,test-aes.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-base64: $(call BUILDOBJ,test-base64.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-https: $(call BUILDOBJ,test-https.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $< $(LLIBS)

test-https_server: $(call BUILDOBJ,test-https_server.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $< $(LLIBS)

test-list: $(call BUILDOBJ,test-list.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-md4: $(call BUILDOBJ,test-md4.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-milenage: $(call BUILDOBJ,test-milenage.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-rc4: $(call BUILDOBJ,test-rc4.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-rsa-sig-ver: $(call BUILDOBJ,test-rsa-sig-ver.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $< $(LLIBS)

test-sha1: $(call BUILDOBJ,test-sha1.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-sha256: $(call BUILDOBJ,test-sha256.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $^ $(LLIBS)

test-x509v3: $(call BUILDOBJ,test-x509v3.o) $(LIBS)
	$(LDO) $(LDFLAGS) -o $@ $< $(LLIBS)


run-tests: $(ALL)
	./test-aes
	./test-list
	./test-md4
	./test-milenage
	./test-rsa-sig-ver
	./test-sha1
	./test-sha256
	@echo
	@echo All tests completed successfully.

clean: common-clean
	rm -f *~
	rm -f test_x509v3_nist.out.*
	rm -f test_x509v3_nist2.out.*
