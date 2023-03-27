# Makefile fragment - requires GNU make
#
# Copyright (c) 2019-2023, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

PLM := $(srcdir)/pl/math
AOR := $(srcdir)/math
B := build/pl/math

math-lib-srcs := $(wildcard $(PLM)/*.[cS])
math-test-srcs := \
	$(AOR)/test/mathtest.c \
	$(AOR)/test/mathbench.c \
	$(AOR)/test/ulp.c \

math-test-host-srcs := $(wildcard $(AOR)/test/rtest/*.[cS])

math-includes := $(patsubst $(PLM)/%,build/pl/%,$(wildcard $(PLM)/include/*.h))
math-test-includes := $(patsubst $(PLM)/%,build/pl/include/%,$(wildcard $(PLM)/test/*.h))

math-libs := \
	build/pl/lib/libmathlib.so \
	build/pl/lib/libmathlib.a \

math-tools := \
	build/pl/bin/mathtest \
	build/pl/bin/mathbench \
	build/pl/bin/mathbench_libc \
	build/pl/bin/runulp.sh \
	build/pl/bin/ulp \

math-host-tools := \
	build/pl/bin/rtest \

math-lib-objs := $(patsubst $(PLM)/%,$(B)/%.o,$(basename $(math-lib-srcs)))
math-test-objs := $(patsubst $(AOR)/%,$(B)/%.o,$(basename $(math-test-srcs)))
math-host-objs := $(patsubst $(AOR)/%,$(B)/%.o,$(basename $(math-test-host-srcs)))
math-target-objs := $(math-lib-objs) $(math-test-objs)
math-objs := $(math-target-objs) $(math-target-objs:%.o=%.os) $(math-host-objs)

pl/math-files := \
	$(math-objs) \
	$(math-libs) \
	$(math-tools) \
	$(math-host-tools) \
	$(math-includes) \
	$(math-test-includes) \

all-pl/math: $(math-libs) $(math-tools) $(math-includes) $(math-test-includes)

$(math-objs): $(math-includes) $(math-test-includes)
$(math-objs): CFLAGS_PL += $(math-cflags)
$(B)/test/mathtest.o: CFLAGS_PL += -fmath-errno
$(math-host-objs): CC = $(HOST_CC)
$(math-host-objs): CFLAGS_PL = $(HOST_CFLAGS)

build/pl/include/test/ulp_funcs_gen.h: $(math-lib-srcs)
	# Replace PL_SIG
	cat $^ | grep PL_SIG | $(CC) -xc - -o - -E "-DPL_SIG(v, t, a, f, ...)=_Z##v##t##a(f)" -P > $@

build/pl/include/test/mathbench_funcs_gen.h: $(math-lib-srcs)
	# Replace PL_SIG macros with mathbench func entries
	cat $^ | grep PL_SIG | $(CC) -xc - -o - -E "-DPL_SIG(v, t, a, f, ...)=_Z##v##t##a(f, ##__VA_ARGS__)" -P > $@

build/pl/include/test/ulp_wrappers_gen.h: $(math-lib-srcs)
	# Replace PL_SIG macros with ULP wrapper declarations
	cat $^ | grep PL_SIG | $(CC) -xc - -o - -E "-DPL_SIG(v, t, a, f, ...)=Z##v##N##t##a##_WRAP(f)" -P > $@

$(B)/test/ulp.o: $(AOR)/test/ulp.h build/pl/include/test/ulp_funcs_gen.h build/pl/include/test/ulp_wrappers_gen.h
$(B)/test/ulp.o: CFLAGS_PL += -I build/pl/include/test

$(B)/test/mathbench.o: build/pl/include/test/mathbench_funcs_gen.h
$(B)/test/mathbench.o: CFLAGS_PL += -I build/pl/include/test

build/pl/lib/libmathlib.so: $(math-lib-objs:%.o=%.os)
	$(CC) $(CFLAGS_PL) $(LDFLAGS) -shared -o $@ $^

build/pl/lib/libmathlib.a: $(math-lib-objs)
	rm -f $@
	$(AR) rc $@ $^
	$(RANLIB) $@

$(math-host-tools): HOST_LDLIBS += -lm -lmpfr -lmpc
$(math-tools): LDLIBS += $(math-ldlibs) -lm

# Some targets to build pl/math/test from math/test sources
build/pl/math/test/%.o: $(srcdir)/math/test/%.S
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/math/test/%.o: $(srcdir)/math/test/%.c
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/math/test/%.os: $(srcdir)/math/test/%.S
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/math/test/%.os: $(srcdir)/math/test/%.c
	$(CC) $(CFLAGS_PL) -c -o $@ $<

# Some targets to build pl/ sources using appropriate flags
build/pl/%.o: $(srcdir)/pl/%.S
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/%.o: $(srcdir)/pl/%.c
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/%.os: $(srcdir)/pl/%.S
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/%.os: $(srcdir)/pl/%.c
	$(CC) $(CFLAGS_PL) -c -o $@ $<

build/pl/bin/rtest: $(math-host-objs)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_LDFLAGS) -o $@ $^ $(HOST_LDLIBS)

build/pl/bin/mathtest: $(B)/test/mathtest.o build/pl/lib/libmathlib.a
	$(CC) $(CFLAGS_PL) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

build/pl/bin/mathbench: $(B)/test/mathbench.o build/pl/lib/libmathlib.a
	$(CC) $(CFLAGS_PL) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

# This is not ideal, but allows custom symbols in mathbench to get resolved.
build/pl/bin/mathbench_libc: $(B)/test/mathbench.o build/pl/lib/libmathlib.a
	$(CC) $(CFLAGS_PL) $(LDFLAGS) -static -o $@ $< $(LDLIBS) -lc build/pl/lib/libmathlib.a -lm

build/pl/bin/ulp: $(B)/test/ulp.o build/pl/lib/libmathlib.a
	$(CC) $(CFLAGS_PL) $(LDFLAGS) -static -o $@ $^ $(LDLIBS)

build/pl/include/%.h: $(PLM)/include/%.h
	cp $< $@

build/pl/include/test/%.h: $(PLM)/test/%.h
	cp $< $@

build/pl/bin/%.sh: $(PLM)/test/%.sh
	cp $< $@

pl-math-tests := $(wildcard $(PLM)/test/testcases/directed/*.tst)
pl-math-rtests := $(wildcard $(PLM)/test/testcases/random/*.tst)

check-pl/math-test: $(math-tools)
	cat $(pl-math-tests) | $(EMULATOR) build/pl/bin/mathtest $(math-testflags)

check-pl/math-rtest: $(math-host-tools) $(math-tools)
	cat $(pl-math-rtests) | build/pl/bin/rtest | $(EMULATOR) build/pl/bin/mathtest $(math-testflags)

ulp-input-dir=$(B)/test/inputs

math-lib-lims = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.ulp,$(basename $(math-lib-srcs)))
math-lib-aliases = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.alias,$(basename $(math-lib-srcs)))
math-lib-fenvs = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.fenv,$(basename $(math-lib-srcs)))
math-lib-itvs = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.itv,$(basename $(math-lib-srcs)))

ulp-inputs = $(math-lib-lims) $(math-lib-aliases) $(math-lib-fenvs) $(math-lib-itvs)

$(ulp-inputs): CFLAGS_PL += -I$(PLM) -I$(PLM)/include $(math-cflags)

$(ulp-input-dir)/%.ulp: $(PLM)/%.c
	mkdir -p $(@D)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep -o "PL_TEST_ULP [^ ]* [^ ]*" || true; } > $@

$(ulp-input-dir)/%.alias: $(PLM)/%.c
	mkdir -p $(@D)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep "PL_TEST_ALIAS" || true; } | sed "s/_x / /g"> $@

$(ulp-input-dir)/%.fenv: $(PLM)/%.c
	mkdir -p $(@D)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep -o "PL_TEST_EXPECT_FENV_ENABLED [^ ]*" || true; } > $@

$(ulp-input-dir)/%.itv: $(PLM)/%.c
	mkdir -p $(dir $@)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep "PL_TEST_INTERVAL " || true; } | sed "s/ PL_TEST_INTERVAL/\nPL_TEST_INTERVAL/g" > $@

ulp-lims := $(ulp-input-dir)/limits
$(ulp-lims): $(math-lib-lims)
	cat $^ | sed "s/PL_TEST_ULP //g;s/^ *//g" > $@

ulp-aliases := $(ulp-input-dir)/aliases
$(ulp-aliases): $(math-lib-aliases)
	cat $^ | sed "s/PL_TEST_ALIAS //g;s/^ *//g" > $@

fenv-exps := $(ulp-input-dir)/fenv
$(fenv-exps): $(math-lib-fenvs)
	cat $^ | sed "s/PL_TEST_EXPECT_FENV_ENABLED //g;s/^ *//g" > $@

ulp-itvs-noalias := $(ulp-input-dir)/itvs_noalias
$(ulp-itvs-noalias): $(math-lib-itvs)
	cat $^ > $@

rename-aliases := $(ulp-input-dir)/rename_alias.sed
$(rename-aliases): $(ulp-aliases)
	# Build sed script for replacing aliases from generated alias file
	cat $< |  awk '{ print "s/ " $$1 " / " $$2 " /g" }' > $@

ulp-itvs-alias := $(ulp-input-dir)/itvs_alias
$(ulp-itvs-alias): $(ulp-itvs-noalias) $(rename-aliases)
	cat $< | sed  -f $(rename-aliases) > $@

ulp-itvs := $(ulp-input-dir)/intervals
$(ulp-itvs): $(ulp-itvs-alias) $(ulp-itvs-noalias)
	cat $^ | sort -u | sed "s/PL_TEST_INTERVAL //g" > $@

check-pl/math-ulp: $(math-tools) $(ulp-lims) $(ulp-aliases) $(fenv-exps) $(ulp-itvs)
	WANT_SVE_MATH=$(WANT_SVE_MATH) \
	ULPFLAGS="$(math-ulpflags)" \
	LIMITS=../../../$(ulp-lims) \
	ALIASES=../../../$(ulp-aliases) \
	INTERVALS=../../../$(ulp-itvs) \
	FENV=../../../$(fenv-exps) \
	build/pl/bin/runulp.sh $(EMULATOR)

check-pl/math: check-pl/math-test check-pl/math-rtest check-pl/math-ulp

$(DESTDIR)$(libdir)/pl/%.so: build/pl/lib/%.so
	$(INSTALL) -D $< $@

$(DESTDIR)$(libdir)/pl/%: build/pl/lib/%
	$(INSTALL) -m 644 -D $< $@

$(DESTDIR)$(includedir)/pl/%: build/pl/include/%
	$(INSTALL) -m 644 -D $< $@

install-pl/math: \
 $(math-libs:build/pl/lib/%=$(DESTDIR)$(libdir)/pl/%) \
 $(math-includes:build/pl/include/%=$(DESTDIR)$(includedir)/pl/%)

clean-pl/math:
	rm -f $(pl/math-files)

.PHONY: all-pl/math check-pl/math-test check-pl/math-rtest check-pl/math-ulp check-pl/math install-pl/math clean-pl/math
