# Makefile fragment - requires GNU make
#
# Copyright (c) 2019-2021, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

S := $(srcdir)/string
B := build/string

ifeq ($(ARCH),)
all-string bench-string check-string install-string clean-string:
	@echo "*** Please set ARCH in config.mk. ***"
	@exit 1
else

string-lib-srcs := $(wildcard $(S)/$(ARCH)/*.[cS])
string-lib-srcs += $(wildcard $(S)/$(ARCH)/experimental/*.[cS])
string-test-srcs := $(wildcard $(S)/test/*.c)
string-bench-srcs := $(wildcard $(S)/bench/*.c)

string-arch-include-dir := $(wildcard $(S)/$(ARCH))
string-arch-includes := $(wildcard $(S)/$(ARCH)/*.h)
string-includes := $(patsubst $(S)/%,build/%,$(wildcard $(S)/include/*.h))

string-libs := \
	build/lib/libstringlib.so \
	build/lib/libstringlib.a \

string-tests := \
	build/bin/test/memcpy \
	build/bin/test/memmove \
	build/bin/test/memset \
	build/bin/test/memchr \
	build/bin/test/memrchr \
	build/bin/test/memcmp \
	build/bin/test/__mtag_tag_region \
	build/bin/test/__mtag_tag_zero_region \
	build/bin/test/strcpy \
	build/bin/test/stpcpy \
	build/bin/test/strcmp \
	build/bin/test/strchr \
	build/bin/test/strrchr \
	build/bin/test/strchrnul \
	build/bin/test/strlen \
	build/bin/test/strnlen \
	build/bin/test/strncmp

string-benches := \
	build/bin/bench/memcpy \
	build/bin/bench/memset \
	build/bin/bench/strlen

string-lib-objs := $(patsubst $(S)/%,$(B)/%.o,$(basename $(string-lib-srcs)))
string-test-objs := $(patsubst $(S)/%,$(B)/%.o,$(basename $(string-test-srcs)))
string-bench-objs := $(patsubst $(S)/%,$(B)/%.o,$(basename $(string-bench-srcs)))

string-objs := \
	$(string-lib-objs) \
	$(string-lib-objs:%.o=%.os) \
	$(string-test-objs) \
	$(string-bench-objs)

string-files := \
	$(string-objs) \
	$(string-libs) \
	$(string-tests) \
	$(string-benches) \
	$(string-includes) \

all-string: $(string-libs) $(string-tests) $(string-benches) $(string-includes)

$(string-objs): $(string-includes) $(string-arch-includes)
$(string-objs): CFLAGS_ALL += $(string-cflags) -I$(string-arch-include-dir)

$(string-test-objs): CFLAGS_ALL += -D_GNU_SOURCE

build/lib/libstringlib.so: $(string-lib-objs:%.o=%.os)
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -shared -o $@ $^

build/lib/libstringlib.a: $(string-lib-objs)
	rm -f $@
	$(AR) rc $@ $^
	$(RANLIB) $@

build/bin/test/%: $(B)/test/%.o build/lib/libstringlib.a
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

build/bin/bench/%: $(B)/bench/%.o build/lib/libstringlib.a
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

build/include/%.h: $(S)/include/%.h
	cp $< $@

build/bin/%.sh: $(S)/test/%.sh
	cp $< $@

string-tests-out = $(string-tests:build/bin/test/%=build/string/test/%.out)

build/string/test/%.out: build/bin/test/%
	$(EMULATOR) $^ | tee $@.tmp
	mv $@.tmp $@

check-string: $(string-tests-out)
	! grep FAIL $^

bench-string: $(string-benches)
	$(EMULATOR) build/bin/bench/strlen
	$(EMULATOR) build/bin/bench/memcpy
	$(EMULATOR) build/bin/bench/memset

install-string: \
 $(string-libs:build/lib/%=$(DESTDIR)$(libdir)/%) \
 $(string-includes:build/include/%=$(DESTDIR)$(includedir)/%)

clean-string:
	rm -f $(string-files)
endif

.PHONY: all-string bench-string check-string install-string clean-string
