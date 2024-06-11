# Makefile fragment - requires GNU make
#
# Copyright (c) 2019-2024, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

PLM := $(srcdir)/pl/math
AOR := $(srcdir)/math
B := build/pl/math

pl-lib-srcs := $(wildcard $(PLM)/*.[cS])

ifeq ($(WANT_SVE_MATH), 0)
pl-lib-srcs := $(filter-out $(PLM)/sv_%, $(pl-lib-srcs))
endif

math-test-srcs := \
	$(AOR)/test/mathtest.c \
	$(AOR)/test/mathbench.c \
	$(AOR)/test/ulp.c \

math-test-host-srcs := $(wildcard $(AOR)/test/rtest/*.[cS])

pl-includes := $(patsubst $(PLM)/%,build/pl/%,$(wildcard $(PLM)/include/*.h))
pl-test-includes := $(patsubst $(PLM)/%,build/pl/include/%,$(wildcard $(PLM)/test/*.h))

pl-libs := \
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

pl-lib-objs := $(patsubst $(PLM)/%,$(B)/%.o,$(basename $(pl-lib-srcs)))
math-test-objs := $(patsubst $(AOR)/%,$(B)/%.o,$(basename $(math-test-srcs)))
math-host-objs := $(patsubst $(AOR)/%,$(B)/%.o,$(basename $(math-test-host-srcs)))
pl-target-objs := $(pl-lib-objs) $(math-test-objs)
pl-objs := $(pl-target-objs) $(pl-target-objs:%.o=%.os) $(math-host-objs)

pl/math-files := \
	$(pl-objs) \
	$(pl-libs) \
	$(math-tools) \
	$(math-host-tools) \
	$(pl-includes) \
	$(pl-test-includes) \

all-pl/math: $(pl-libs) $(math-tools) $(pl-includes) $(pl-test-includes)

$(pl-objs): $(pl-includes) $(pl-test-includes)
$(pl-objs): CFLAGS_PL += $(math-cflags)
$(B)/test/mathtest.o: CFLAGS_PL += -fmath-errno
$(math-host-objs): CC = $(HOST_CC)
$(math-host-objs): CFLAGS_PL = $(HOST_CFLAGS)

$(B)/sv_%: CFLAGS_PL += $(math-sve-cflags)

build/pl/include/test/ulp_funcs_gen.h: $(pl-lib-srcs)
	# Replace PL_SIG
	cat $^ | grep PL_SIG | $(CC) -xc - -o - -E "-DPL_SIG(v, t, a, f, ...)=_Z##v##t##a(f)" -P > $@

build/pl/include/test/mathbench_funcs_gen.h: $(pl-lib-srcs)
	# Replace PL_SIG macros with mathbench func entries
	cat $^ | grep PL_SIG | $(CC) -xc - -o - -E "-DPL_SIG(v, t, a, f, ...)=_Z##v##t##a(f, ##__VA_ARGS__)" -P > $@

build/pl/include/test/ulp_wrappers_gen.h: $(pl-lib-srcs)
	# Replace PL_SIG macros with ULP wrapper declarations
	cat $^ | grep PL_SIG | $(CC) -xc - -o - -E "-DPL_SIG(v, t, a, f, ...)=Z##v##N##t##a##_WRAP(f)" -P > $@

$(B)/test/ulp.o: $(AOR)/test/ulp.h build/pl/include/test/ulp_funcs_gen.h build/pl/include/test/ulp_wrappers_gen.h
$(B)/test/ulp.o: CFLAGS_PL += -I build/pl/include/test

$(B)/test/mathbench.o: build/pl/include/test/mathbench_funcs_gen.h
$(B)/test/mathbench.o: CFLAGS_PL += -I build/pl/include/test

build/pl/lib/libmathlib.so: $(pl-lib-objs:%.o=%.os)
	$(CC) $(CFLAGS_PL) $(LDFLAGS) -shared -o $@ $^

build/pl/lib/libmathlib.a: $(pl-lib-objs)
	rm -f $@
	$(AR) rc $@ $^
	$(RANLIB) $@

$(math-host-tools): HOST_LDLIBS += -lm -lmpfr -lmpc
$(math-tools): LDLIBS += $(math-ldlibs) -lm
# math-sve-cflags should be empty if WANT_SVE_MATH is not enabled
$(math-tools): CFLAGS_PL += $(math-sve-cflags)

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

math-lib-lims = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.ulp,$(basename $(pl-lib-srcs)))
math-lib-fenvs = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.fenv,$(basename $(pl-lib-srcs)))
math-lib-itvs = $(patsubst $(PLM)/%,$(ulp-input-dir)/%.itv,$(basename $(pl-lib-srcs)))

ulp-inputs = $(math-lib-lims) $(math-lib-fenvs) $(math-lib-itvs)

$(ulp-inputs): CFLAGS_PL += -I$(PLM) -I$(PLM)/include $(math-cflags)

$(ulp-input-dir)/%.ulp: $(PLM)/%.c
	mkdir -p $(@D)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep -o "PL_TEST_ULP [^ ]* [^ ]*" || true; } > $@

$(ulp-input-dir)/%.fenv: $(PLM)/%.c
	mkdir -p $(@D)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep -o "PL_TEST_EXPECT_FENV_ENABLED [^ ]*" || true; } > $@

$(ulp-input-dir)/%.itv: $(PLM)/%.c
	mkdir -p $(dir $@)
	$(CC) -I$(PLM)/test $(CFLAGS_PL) $< -o - -E | { grep "PL_TEST_INTERVAL " || true; } | sed "s/ PL_TEST_INTERVAL/\nPL_TEST_INTERVAL/g" > $@

ulp-lims := $(ulp-input-dir)/limits
$(ulp-lims): $(math-lib-lims)
	cat $^ | sed "s/PL_TEST_ULP //g;s/^ *//g" > $@

fenv-exps := $(ulp-input-dir)/fenv
$(fenv-exps): $(math-lib-fenvs)
	cat $^ | sed "s/PL_TEST_EXPECT_FENV_ENABLED //g;s/^ *//g" > $@

ulp-itvs := $(ulp-input-dir)/intervals
$(ulp-itvs): $(math-lib-itvs)
	cat $^ | sort -u | sed "s/PL_TEST_INTERVAL //g" > $@

check-pl/math-ulp: $(math-tools) $(ulp-lims) $(fenv-exps) $(ulp-itvs)
	WANT_SVE_MATH=$(WANT_SVE_MATH) \
	ULPFLAGS="$(math-ulpflags)" \
	LIMITS=../../../$(ulp-lims) \
	INTERVALS=../../../$(ulp-itvs) \
	FENV=../../../$(fenv-exps) \
	FUNC=$(func) \
	build/pl/bin/runulp.sh $(EMULATOR)

check-pl/math: check-pl/math-test check-pl/math-rtest check-pl/math-ulp

$(DESTDIR)$(libdir)/pl/%.so: build/pl/lib/%.so
	$(INSTALL) -D $< $@

$(DESTDIR)$(libdir)/pl/%: build/pl/lib/%
	$(INSTALL) -m 644 -D $< $@

$(DESTDIR)$(includedir)/pl/%: build/pl/include/%
	$(INSTALL) -m 644 -D $< $@

install-pl/math: \
 $(pl-libs:build/pl/lib/%=$(DESTDIR)$(libdir)/pl/%) \
 $(pl-includes:build/pl/include/%=$(DESTDIR)$(includedir)/pl/%)

clean-pl/math:
	rm -f $(pl/math-files)

.PHONY: all-pl/math check-pl/math-test check-pl/math-rtest check-pl/math-ulp check-pl/math install-pl/math clean-pl/math
