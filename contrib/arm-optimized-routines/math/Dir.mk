# Makefile fragment - requires GNU make
#
# Copyright (c) 2019-2024, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

.SECONDEXPANSION:

ifneq ($(OS),Linux)
  ifeq ($(WANT_SIMD_EXCEPT),1)
    $(error WANT_SIMD_EXCEPT is not supported outside Linux)
  endif
  ifneq ($(USE_MPFR),1)
    $(warning WARNING: Double-precision ULP tests will not be usable without MPFR)
  endif
  ifeq ($(USE_GLIBC_ABI),1)
    $(error Can only generate special GLIBC symbols on Linux - please disable USE_GLIBC_ABI)
  endif
endif

ifneq ($(ARCH),aarch64)
  ifeq ($(WANT_TRIGPI_TESTS),1)
    $(error trigpi functions only supported on aarch64)
  endif
  ifeq ($(WANT_EXPERIMENTAL_MATH),1)
    $(error Experimental math only supported on aarch64)
  endif
endif

math-src-dir := $(srcdir)/math
math-build-dir := build/math

math-lib-srcs := $(wildcard $(math-src-dir)/*.[cS])
math-lib-srcs += $(wildcard $(math-src-dir)/$(ARCH)/*.[cS])
ifeq ($(OS),Linux)
# Vector symbols only supported on Linux
math-lib-srcs += $(wildcard $(math-src-dir)/$(ARCH)/*/*.[cS])
endif

ifeq ($(WANT_EXPERIMENTAL_MATH), 1)
ifeq ($(OS),Linux)
# Vector symbols only supported on Linux
math-lib-srcs += $(wildcard $(math-src-dir)/$(ARCH)/experimental/*/*.[cS])
else
math-lib-srcs += $(wildcard $(math-src-dir)/$(ARCH)/experimental/*.[cS])
endif
else
# Scalar experimental symbols will have been added by wildcard, so remove them
math-lib-srcs := $(filter-out $(math-src-dir)/aarch64/experimental/%, $(math-lib-srcs))
endif

math-test-srcs := \
	$(math-src-dir)/test/mathtest.c \
	$(math-src-dir)/test/mathbench.c \
	$(math-src-dir)/test/ulp.c \

math-test-host-srcs := $(wildcard $(math-src-dir)/test/rtest/*.[cS])

math-includes := $(patsubst $(math-src-dir)/%,build/%,$(wildcard $(math-src-dir)/include/*.h))

math-libs := \
	build/lib/libmathlib.so \
	build/lib/libmathlib.a \

math-tools := \
	build/bin/mathtest \
	build/bin/mathbench \
	build/bin/mathbench_libc \
	build/bin/runulp.sh \
	build/bin/ulp \

math-host-tools := \
	build/bin/rtest \

math-lib-objs := $(patsubst $(math-src-dir)/%,$(math-build-dir)/%.o,$(basename $(math-lib-srcs)))
math-test-objs := $(patsubst $(math-src-dir)/%,$(math-build-dir)/%.o,$(basename $(math-test-srcs)))
math-host-objs := $(patsubst $(math-src-dir)/%,$(math-build-dir)/%.o,$(basename $(math-test-host-srcs)))
math-target-objs := $(math-lib-objs) $(math-test-objs)
math-objs := $(math-target-objs) $(math-target-objs:%.o=%.os) $(math-host-objs)

math-files := \
	$(math-objs) \
	$(math-libs) \
	$(math-tools) \
	$(math-host-tools) \
	$(math-includes)

all-math: $(math-libs) $(math-tools) $(math-includes)

$(math-objs): $(math-includes)
$(math-objs): CFLAGS_ALL += $(math-cflags)
$(math-build-dir)/test/mathtest.o: CFLAGS_ALL += -fmath-errno
$(math-host-objs): CC = $(HOST_CC)
$(math-host-objs): CFLAGS_ALL = $(HOST_CFLAGS)

# Add include path for experimental routines so they can share helpers with non-experimental
$(math-build-dir)/aarch64/experimental/advsimd/%: CFLAGS_ALL += -I$(math-src-dir)/aarch64/advsimd
$(math-build-dir)/aarch64/experimental/sve/%: CFLAGS_ALL += -I$(math-src-dir)/aarch64/sve

$(math-objs): CFLAGS_ALL += -I$(math-src-dir)

ulp-funcs-dir = build/test/ulp-funcs/
ulp-wrappers-dir = build/test/ulp-wrappers/
mathbench-funcs-dir = build/test/mathbench-funcs/
test-sig-dirs = $(ulp-funcs-dir) $(ulp-wrappers-dir) $(mathbench-funcs-dir)
build/include/test $(test-sig-dirs) $(addsuffix /$(ARCH),$(test-sig-dirs)) $(addsuffix /aarch64/experimental,$(test-sig-dirs)) \
$(addsuffix /aarch64/experimental/advsimd,$(test-sig-dirs)) $(addsuffix /aarch64/experimental/sve,$(test-sig-dirs)) \
$(addsuffix /aarch64/advsimd,$(test-sig-dirs)) $(addsuffix /aarch64/sve,$(test-sig-dirs)):
	mkdir -p $@

ulp-funcs = $(patsubst $(math-src-dir)/%,$(ulp-funcs-dir)/%,$(basename $(math-lib-srcs)))
ulp-wrappers = $(patsubst $(math-src-dir)/%,$(ulp-wrappers-dir)/%,$(basename $(math-lib-srcs)))
mathbench-funcs = $(patsubst $(math-src-dir)/%,$(mathbench-funcs-dir)/%,$(basename $(math-lib-srcs)))

ifeq ($(WANT_SVE_TESTS), 0)
  # Filter out anything with sve in the path
  ulp-funcs := $(foreach a,$(ulp-funcs),$(if $(findstring sve,$a),,$a))
  ulp-wrappers := $(foreach a,$(ulp-wrappers),$(if $(findstring sve,$a),,$a))
  mathbench-funcs := $(foreach a,$(mathbench-funcs),$(if $(findstring sve,$a),,$a))
endif

define emit_sig
$1/aarch64/experimental/sve/%.i: EXTRA_INC = -I$(math-src-dir)/aarch64/sve
$1/aarch64/experimental/advsimd/%.i: EXTRA_INC = -I$(math-src-dir)/aarch64/advsimd
$1/%.i: $(math-src-dir)/%.c | $$$$(@D)
	$(CC) $$< $(math-cflags) -I$(math-src-dir)/include -I$(math-src-dir) $$(EXTRA_INC) -D$2 -E -o $$@
$1/%: $1/%.i
	{ grep TEST_SIG $$< || true; } | cut -f 2- -d ' ' > $$@
endef

$(eval $(call emit_sig,$(ulp-funcs-dir),EMIT_ULP_FUNCS))
$(eval $(call emit_sig,$(ulp-wrappers-dir),EMIT_ULP_WRAPPERS))
$(eval $(call emit_sig,$(mathbench-funcs-dir),EMIT_MATHBENCH_FUNCS))

ulp-funcs-gen = build/include/test/ulp_funcs_gen.h
ulp-wrappers-gen = build/include/test/ulp_wrappers_gen.h
mathbench-funcs-gen = build/include/test/mathbench_funcs_gen.h
math-tools-autogen-headers = $(ulp-funcs-gen) $(ulp-wrappers-gen) $(mathbench-funcs-gen)

$(ulp-funcs-gen): $(ulp-funcs) | $$(@D)
$(ulp-wrappers-gen): $(ulp-wrappers) | $$(@D)
$(mathbench-funcs-gen): $(mathbench-funcs) | $$(@D)

$(math-tools-autogen-headers): | $$(@D)
	cat $^ | sort -u > $@

$(math-build-dir)/test/mathbench.o: $(mathbench-funcs-gen)
$(math-build-dir)/test/ulp.o: $(math-src-dir)/test/ulp.h $(ulp-funcs-gen) $(ulp-wrappers-gen)

build/lib/libmathlib.so: $(math-lib-objs:%.o=%.os)
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -shared -o $@ $^

build/lib/libmathlib.a: $(math-lib-objs)
	rm -f $@
	$(AR) rc $@ $^
	$(RANLIB) $@

$(math-host-tools): HOST_LDLIBS += $(libm-libs) $(mpfr-libs) $(mpc-libs)
$(math-tools): LDLIBS += $(math-ldlibs) $(libm-libs)

ifneq ($(OS),Darwin)
  $(math-tools): LDFLAGS += -static
endif

build/bin/rtest: $(math-host-objs)
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_LDFLAGS) -o $@ $^ $(HOST_LDLIBS)

build/bin/mathtest: $(math-build-dir)/test/mathtest.o build/lib/libmathlib.a
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -o $@ $^ $(libm-libs)

build/bin/mathbench: $(math-build-dir)/test/mathbench.o build/lib/libmathlib.a
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -o $@ $^ $(libm-libs)

# This is not ideal, but allows custom symbols in mathbench to get resolved.
build/bin/mathbench_libc: $(math-build-dir)/test/mathbench.o build/lib/libmathlib.a
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -o $@ $< $(libm-libs) $(libc-libs) build/lib/libmathlib.a $(libm-libs)

build/bin/ulp: $(math-build-dir)/test/ulp.o build/lib/libmathlib.a
	$(CC) $(CFLAGS_ALL) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/include/%.h: $(math-src-dir)/include/%.h
	cp $< $@

build/bin/%.sh: $(math-src-dir)/test/%.sh
	cp $< $@

math-tests := $(wildcard $(math-src-dir)/test/testcases/directed/*.tst)
ifneq ($(WANT_EXP10_TESTS),1)
math-tests := $(filter-out %exp10.tst, $(math-tests))
endif
math-rtests := $(wildcard $(math-src-dir)/test/testcases/random/*.tst)

check-math-test: $(math-tools)
	cat $(math-tests) | $(EMULATOR) build/bin/mathtest $(math-testflags)

check-math-rtest: $(math-host-tools) $(math-tools)
	cat $(math-rtests) | build/bin/rtest | $(EMULATOR) build/bin/mathtest $(math-testflags)

ulp-input-dir = $(math-build-dir)/test/inputs
$(ulp-input-dir) $(ulp-input-dir)/$(ARCH) $(ulp-input-dir)/aarch64/sve $(ulp-input-dir)/aarch64/advsimd \
$(ulp-input-dir)/aarch64/experimental $(ulp-input-dir)/aarch64/experimental/advsimd $(ulp-input-dir)/aarch64/experimental/sve:
	mkdir -p $@

math-lib-lims = $(patsubst $(math-src-dir)/%.c,$(ulp-input-dir)/%.ulp,$(math-lib-srcs))
math-lib-lims-nn = $(patsubst $(math-src-dir)/%.c,$(ulp-input-dir)/%.ulp_nn,$(math-lib-srcs))
math-lib-fenvs = $(patsubst $(math-src-dir)/%.c,$(ulp-input-dir)/%.fenv,$(math-lib-srcs))
math-lib-itvs = $(patsubst $(math-src-dir)/%.c,$(ulp-input-dir)/%.itv,$(math-lib-srcs))
math-lib-cvals = $(patsubst $(math-src-dir)/%.c,$(ulp-input-dir)/%.cval,$(math-lib-srcs))

ulp-inputs = $(math-lib-lims) $(math-lib-lims-nn) $(math-lib-fenvs) $(math-lib-itvs) $(math-lib-cvals)
$(ulp-inputs): CFLAGS = -I$(math-src-dir)/test -I$(math-src-dir)/include -I$(math-src-dir) $(math-cflags)\
                        -I$(math-src-dir)/aarch64/advsimd -I$(math-src-dir)/aarch64/sve

$(ulp-input-dir)/%.ulp.i: $(math-src-dir)/%.c | $$(@D)
	$(CC) $(CFLAGS) $< -E -o $@

$(ulp-input-dir)/%.ulp: $(ulp-input-dir)/%.ulp.i
	{ grep "TEST_ULP " $< || true; } > $@

$(ulp-input-dir)/%.ulp_nn.i: $(math-src-dir)/%.c | $$(@D)
	$(CC) $(CFLAGS) $< -E -o $@

$(ulp-input-dir)/%.ulp_nn: $(ulp-input-dir)/%.ulp_nn.i
	{ grep "TEST_ULP_NONNEAREST " $< || true; } > $@

$(ulp-input-dir)/%.fenv.i: $(math-src-dir)/%.c | $$(@D)
	$(CC) $(CFLAGS) $< -E -o $@

$(ulp-input-dir)/%.fenv: $(ulp-input-dir)/%.fenv.i
	{ grep "TEST_DISABLE_FENV " $< || true; } > $@

$(ulp-input-dir)/%.itv.i: $(math-src-dir)/%.c | $$(@D)
	$(CC) $(CFLAGS) $< -E -o $@

$(ulp-input-dir)/%.itv: $(ulp-input-dir)/%.itv.i
	{ grep "TEST_INTERVAL " $< || true; } | sed "s/ TEST_INTERVAL/\nTEST_INTERVAL/g" > $@

$(ulp-input-dir)/%.cval.i: $(math-src-dir)/%.c | $$(@D)
	$(CC) $(CFLAGS) $< -E -o $@

$(ulp-input-dir)/%.cval: $(ulp-input-dir)/%.cval.i
	{ grep "TEST_CONTROL_VALUE " $< || true; } > $@

ulp-lims = $(ulp-input-dir)/limits
$(ulp-lims): $(math-lib-lims)

ulp-lims-nn = $(ulp-input-dir)/limits_nn
$(ulp-lims-nn): $(math-lib-lims-nn)

fenv-exps := $(ulp-input-dir)/fenv
$(fenv-exps): $(math-lib-fenvs)

generic-itvs = $(ulp-input-dir)/itvs
$(generic-itvs): $(filter-out $(ulp-input-dir)/$(ARCH)/%,$(math-lib-itvs))

arch-itvs = $(ulp-input-dir)/$(ARCH)/itvs
$(arch-itvs): $(filter $(ulp-input-dir)/$(ARCH)/%,$(math-lib-itvs))

ulp-cvals := $(ulp-input-dir)/cvals
$(ulp-cvals): $(math-lib-cvals)

# Remove first word, which will be TEST directive
$(ulp-lims) $(ulp-lims-nn) $(fenv-exps) $(arch-itvs) $(generic-itvs) $(ulp-cvals): | $$(@D)
	sed "s/TEST_[^ ]* //g" $^ | sort -u > $@

check-math-ulp: $(ulp-lims) $(ulp-lims-nn)
check-math-ulp: $(fenv-exps) $(ulp-cvals)
check-math-ulp: $(generic-itvs) $(arch-itvs)
check-math-ulp: $(math-tools)
	ULPFLAGS="$(math-ulpflags)" \
	LIMITS=../../$(ulp-lims) \
	ARCH_ITVS=../../$(arch-itvs) \
	GEN_ITVS=../../$(generic-itvs) \
	DISABLE_FENV=../../$(fenv-exps) \
	CVALS=../../$(ulp-cvals) \
	FUNC=$(func) \
	WANT_EXPERIMENTAL_MATH=$(WANT_EXPERIMENTAL_MATH) \
	WANT_SVE_TESTS=$(WANT_SVE_TESTS) \
	USE_MPFR=$(USE_MPFR) \
	build/bin/runulp.sh $(EMULATOR)

check-math: check-math-test check-math-rtest check-math-ulp

install-math: \
 $(math-libs:build/lib/%=$(DESTDIR)$(libdir)/%) \
 $(math-includes:build/include/%=$(DESTDIR)$(includedir)/%)

clean-math:
	rm -f $(math-files)

.PHONY: all-math check-math-test check-math-rtest check-math-ulp check-math install-math clean-math
