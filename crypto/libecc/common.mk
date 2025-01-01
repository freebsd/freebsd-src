# Detect mingw, since some versions throw a warning with the -fPIC option
# (which would be caught as an error in our case with -Werror)
# The ELF PIE related hardening flags are also non sense for Windows
MINGW := $(shell $(CROSS_COMPILE)$(CC) -dumpmachine 2>&1 | grep -v mingw)
# Detect Mac OS compilers: these usually don't like ELF pie related flags ...
APPLE := $(shell $(CROSS_COMPILE)$(CC) -dumpmachine 2>&1 | grep -v apple)
SYS_ROOT :=
ifneq ($(MINGW),)
  FPIC_CFLAG=-fPIC
  ifneq ($(APPLE),)
    FPIE_CFLAG=-fPIE
    FPIE_LDFLAGS=-pie -Wl,-z,relro,-z,now
  endif
endif

ifeq ($(APPLE),)
  SYS_ROOT_PATH := $(shell xcode-select --print-path)
  ifneq ($(SYS_ROOT_PATH),)
    SYS_ROOT_PATH := $(SYS_ROOT_PATH)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    SYS_ROOT := --sysroot=$(SYS_ROOT_PATH)
    $(info Using MacOS SDK $(SYS_ROOT_PATH))
  endif
endif

# NOTE: with mingw, FORTIFY_SOURCE=2 must be used
# in conjuction with stack-protector as check functions
# are implemented in libssp
STACK_PROT_FLAG=-fstack-protector-strong
FORTIFY_FLAGS=-D_FORTIFY_SOURCE=2

# The first goal here is to define a meaningful set of CFLAGS based on compiler,
# debug mode, expected word size (16, 32, 64), etc. Those are then used to
# define two differents kinds of CFLAGS we will use for building our library
# (LIB_CFLAGS) and binaries (BIN_CFLAGS) objects.

# Detect if we are using clang or gcc
CLANG :=  $(shell $(CROSS_COMPILE)$(CC) -v 2>&1 | grep clang)

ifneq ($(CLANG),)
  # get clang version e.g. 14.1.3
  CLANG_VERSION := $(shell $(CROSS_COMPILE)$(CC) -dumpversion)
  # convert to single number e.g. 14 * 100 + 1
  CLANG_VERSION := $(shell echo $(CLANG_VERSION) | cut -f1-2 -d. | sed -e 's/\./*100+/g')
  # Calculate value - e.g. 1401
  CLANG_VERSION := $(shell echo $$(($(CLANG_VERSION))))
  # Comparison results (true if true, empty if false)
  CLANG_VERSION_GTE_12 := $(shell [ $(CLANG_VERSION) -ge 1200 ]  && echo true)
  CLANG_VERSION_GTE_13 := $(shell [ $(CLANG_VERSION) -ge 1300 ]  && echo true)
  CLANG_VERSION_GTE_16 := $(shell [ $(CLANG_VERSION) -ge 1600 ]  && echo true)
  CLANG_VERSION_GTE_17 := $(shell [ $(CLANG_VERSION) -ge 1700 ]  && echo true)
endif

# Default warning flags
# -Werror: treat warnings as errors
#
# Pedantic mode: enable more warnings
# -Wshadow: warn the user if a variable declaration shadows one from a parent context
# -Wdouble-promotion: warn about implicit conversion from float to double
# -Wformat=2: warn about format string vulnerabilities
# -fno-common: disallow global variables with same name and type
# -Wconversion: warn about implicit conversion
# -Wformat-security: warn about format string vulnerabilities
WARNING_CFLAGS = -Werror
ifeq ($(PEDANTIC),1)
  WARNING_CFLAGS += -Wshadow -Wdouble-promotion -Wformat=2 -fno-common -Wconversion -Wformat-security
endif

# Disable certain warnings:
# -Wno-unused-parameter: commonly a false positive. Functions may be required to have a certain signature.
# -Wno-declaration-after-statement: our C standard supports declaration after statements
WARNING_CFLAGS += -Wno-unused-parameter -Wno-declaration-after-statement

# When compiler is *explicitly* set to clang, use its -Weverything option by
# default but disable the sepcific options we cannot support:
#
#   -Wno-reserved-id-macro: our header files use __XXX___ protection macros.
#   -Wno-padded: padding warnings
#   -Wno-packed: warning about packed structure we want to keep that way
#   -Wno-covered-switch-default
#   -Wno-used-but-marked-unused
#
ifneq ($(CLANG),)
  WARNING_CFLAGS += -Weverything \
		 -Wno-reserved-id-macro -Wno-padded \
		 -Wno-packed -Wno-covered-switch-default \
		 -Wno-used-but-marked-unused -Wno-switch-enum
  # Add warnings if we are in pedantic mode
  ifeq ($(PEDANTIC),1)
    WARNING_CFLAGS += -Walloca -Wcast-qual -Wnull-dereference -Wstack-protector -Wvla -Warray-bounds -Warray-bounds-pointer-arithmetic -Wassign-enum -Wbad-function-cast -Wconditional-uninitialized -Wfloat-equal -Wformat-type-confusion -Widiomatic-parentheses -Wimplicit-fallthrough -Wloop-analysis -Wpointer-arith -Wshift-sign-overflow -Wshorten-64-to-32 -Wtautological-constant-in-range-compare -Wunreachable-code-aggressive -Wthread-safety -Wthread-safety-beta -Wcomma
  endif
  ifeq ($(CLANG_VERSION_GTE_13), true)
    # We have to do this because the '_' prefix seems now reserved to builtins
    WARNING_CFLAGS += -Wno-reserved-identifier
  endif
  ifeq ($(CLANG_VERSION_GTE_16), true)
    # NOTE: XXX: this is really a shame to remove this, but
    # we have to wait until this is less sensitive and false positive
    # prone to use it!
    WARNING_CFLAGS += -Wno-unsafe-buffer-usage
  endif
else
  WARNING_CFLAGS += -W -Wextra -Wall -Wunreachable-code
  # Add warnings if we are in pedantic mode
  ifeq ($(PEDANTIC),1)
    WARNING_CFLAGS += -Wpedantic -Wformat-overflow=2 -Wformat-truncation=2 -Wnull-dereference -Wstack-protector -Wtrampolines -Walloca -Wvla -Warray-bounds=2 -Wimplicit-fallthrough=3 -Wshift-overflow=2 -Wcast-qual -Wstringop-overflow=4 -Warith-conversion -Wlogical-op -Wduplicated-cond -Wduplicated-branches -Wformat-signedness -Wstrict-overflow=2 -Wundef -Wstrict-prototypes -Wswitch-default -Wcast-align=strict -Wjump-misses-init
  endif
endif

ifeq ($(WNOERROR), 1)
  # Sometimes "-Werror" might be too much, this can be overriden
  WARNING_CFLAGS := $(subst -Werror,,$(WARNING_CFLAGS))
endif

# If the user has overridden the CFLAGS or LDFLAGS, let's detect it
# and adapt our compilation process
ifdef CFLAGS
USER_DEFINED_CFLAGS = $(CFLAGS)
endif
ifdef LDFLAGS
USER_DEFINED_LDFLAGS = $(LDFLAGS)
endif

CFLAGS ?= $(WARNING_CFLAGS) $(SYS_ROOT) -pedantic -fno-builtin -std=c99 \
	  $(FORTIFY_FLAGS) $(STACK_PROT_FLAG) -O3
LDFLAGS ?=

# Default AR and RANLIB if not overriden by user
AR ?= ar
RANLIB ?= ranlib
# Default AR flags and RANLIB flags if not overriden by user
AR_FLAGS ?= rcs
RANLIB_FLAGS ?=

# Our debug flags
DEBUG_CFLAGS = -DDEBUG -O -g

ifeq ($(VERBOSE_INNER_VALUES),1)
CFLAGS += -DVERBOSE_INNER_VALUES
endif

# Default all and clean target that will be expanded
# later in the Makefile
all:
clean:

debug: CFLAGS += $(DEBUG_CFLAGS)
debug: clean all

# Force 64-bit word size
64: CFLAGS += -DWORDSIZE=64
64: clean all
debug64: CFLAGS += -DWORDSIZE=64 $(DEBUG_CFLAGS)
debug64: clean all

# Force 32-bit word size
32: CFLAGS += -DWORDSIZE=32
32: clean all
debug32: CFLAGS += -DWORDSIZE=32 $(DEBUG_CFLAGS)
debug32: clean all

# Force 16-bit word size
16: CFLAGS += -DWORDSIZE=16
16: clean all
debug16: CFLAGS += -DWORDSIZE=16 $(DEBUG_CFLAGS)
debug16: clean all

# Force to compile with 64-bit arch
force_arch64: CFLAGS += -m64
force_arch64: clean all

# Force to compile with 32-bit arch
force_arch32: CFLAGS += -m32
force_arch32: clean all

# By default, we use an stdlib
ifneq ($(LIBECC_NOSTDLIB),1)
CFLAGS += -DWITH_STDLIB
endif

# Let's now define the two kinds of CFLAGS we will use for building our
# library (LIB_CFLAGS) and binaries (BIN_CFLAGS) objects.
# If the user has not overriden the CFLAGS, we add the usual gcc/clang
# flags to produce binaries compatible with hardening technologies.
ifndef USER_DEFINED_CFLAGS
BIN_CFLAGS  ?= $(CFLAGS) $(FPIE_CFLAG) -MMD -MP
LIB_CFLAGS  ?= $(CFLAGS) $(FPIC_CFLAG) -MMD -MP -ffreestanding
else
BIN_CFLAGS  ?= $(USER_DEFINED_CFLAGS)
LIB_CFLAGS  ?= $(USER_DEFINED_CFLAGS)
endif
ifndef USER_DEFINED_LDFLAGS
BIN_LDFLAGS ?= $(LDFLAGS) $(FPIE_LDFLAGS)
else
BIN_LDFLAGS ?= $(USER_DEFINED_LDFLAGS)
endif

# If the user wants to add extra flags to the existing flags,
# check it and add them
ifdef EXTRA_LIB_CFLAGS
LIB_CFLAGS += $(EXTRA_LIB_CFLAGS)
endif
ifdef EXTRA_LIB_DYN_LDFLAGS
LIB_DYN_LDFLAGS += $(EXTRA_LIB_DYN_LDFLAGS)
endif
ifdef EXTRA_BIN_CFLAGS
BIN_CFLAGS += $(EXTRA_BIN_CFLAGS)
endif
ifdef EXTRA_BIN_LDFLAGS
BIN_LDFLAGS += $(EXTRA_BIN_LDFLAGS)
endif
ifdef EXTRA_CFLAGS
CFLAGS += $(EXTRA_CFLAGS)
endif
ifdef EXTRA_LDFLAGS
LDFLAGS += $(EXTRA_LDFLAGS)
endif

# Add the include folder
LIBECC_INCLUDE_FOLDER = include/
LIB_CFLAGS += -I$(LIBECC_INCLUDE_FOLDER)
BIN_CFLAGS += -I$(LIBECC_INCLUDE_FOLDER)

# Static libraries to produce or link to
LIBARITH = $(BUILD_DIR)/libarith.a
LIBEC = $(BUILD_DIR)/libec.a
LIBSIGN = $(BUILD_DIR)/libsign.a

# Compile dynamic libraries if the user asked to
ifeq ($(WITH_DYNAMIC_LIBS),1)
# Dynamic libraries to produce or link to
LIBARITH_DYN = $(BUILD_DIR)/libarith.so
LIBEC_DYN = $(BUILD_DIR)/libec.so
LIBSIGN_DYN = $(BUILD_DIR)/libsign.so
# The ld flags to generate shared librarie
ifeq ($(APPLE),)
LIB_DYN_LDFLAGS ?= -shared -Wl,-undefined,dynamic_lookup
else
LIB_DYN_LDFLAGS ?= -shared -Wl,-z,relro,-z,now
endif
endif

# Do we want to use blinding to secure signature against some side channels?
ifeq ($(BLINDING),1)
CFLAGS += -DUSE_SIG_BLINDING
endif

# Use complete formulas for point addition and doubling
# NOTE: complete formulas are used as default since they are
# more resilient against side channel attacks and they do not
# have a major performance impact
ifeq ($(COMPLETE),0)
CFLAGS += -DNO_USE_COMPLETE_FORMULAS
endif

# Force Double and Add always usage
ifeq ($(ADALWAYS), 1)
CFLAGS += -DUSE_DOUBLE_ADD_ALWAYS
endif
ifeq ($(ADALWAYS), 0)
CFLAGS += -DUSE_MONTY_LADDER
endif

# Force Montgomery Ladder always usage
ifeq ($(LADDER), 1)
CFLAGS += -DUSE_MONTY_LADDER
endif
ifeq ($(LADDER), 0)
CFLAGS += -DUSE_DOUBLE_ADD_ALWAYS
endif

# Force small stack usage
ifeq ($(SMALLSTACK), 1)
CFLAGS += -DUSE_SMALL_STACK
endif

# Are we sure we will not execute known
# vectors self tests?
ifeq ($(NOKNOWNTESTS), 1)
CFLAGS += -DNO_KNOWN_VECTORS
endif

# Specific version for fuzzing with Cryptofuzz
# Allow raw signature and verification APIs
# which is DANGEROUS. Do not activate in production
# mode!
ifeq ($(CRYPTOFUZZ), 1)
CFLAGS += -DUSE_CRYPTOFUZZ
endif

ifeq ($(ASSERT_PRINT), 1)
CFLAGS += -DUSE_ASSERT_PRINT
endif

# By default, we want to catch all unused functions return values by
# triggering a warning. We deactivate this is we are asked to by the user.
ifneq ($(NO_WARN_UNUSED_RET), 1)
CFLAGS += -DUSE_WARN_UNUSED_RET
endif

# Do we want to use clang or gcc sanitizers?
ifeq ($(USE_SANITIZERS),1)
CFLAGS += -fsanitize=undefined -fsanitize=address -fsanitize=leak
  ifneq ($(CLANG),)
    # Clang version < 12 do not support unsigned-shift-base
    ifeq ($(CLANG_VERSION_GTE_12), true)
      CFLAGS += -fsanitize=integer -fno-sanitize=unsigned-integer-overflow -fno-sanitize=unsigned-shift-base
    endif
  endif
endif

# Do we want to use the ISO14888-3 version of the
# ECRDSA algorithm with discrepancies from the Russian
# RFC references?
ifeq ($(USE_ISO14888_3_ECRDSA),1)
CFLAGS += -DUSE_ISO14888_3_ECRDSA
endif

# Do we have a C++ compiler instead of a C compiler?
GPP := $(shell $(CROSS_COMPILE)$(CC) -v 2>&1 | grep g++)
CLANGPP := $(shell echo $(CROSS_COMPILE)$(CC) | grep clang++)

# g++ case
ifneq ($(GPP),)
CFLAGS := $(patsubst -std=c99, -std=c++2a, $(CFLAGS))
CFLAGS += -Wno-deprecated
# Remove C++ unused pedantic flags
CFLAGS := $(patsubst -Wstrict-prototypes,,$(CFLAGS))
CFLAGS := $(patsubst -Wjump-misses-init,,$(CFLAGS))
CFLAGS := $(patsubst -Wduplicated-branches,,$(CFLAGS))
CFLAGS := $(patsubst -Wno-declaration-after-statement,,$(CFLAGS))
endif
# clang++ case
ifneq ($(CLANGPP),)
CFLAGS := $(patsubst -std=c99, -std=c++2a, $(CFLAGS))
CFLAGS += -Wno-deprecated -Wno-c++98-c++11-c++14-c++17-compat-pedantic -Wno-old-style-cast -Wno-zero-as-null-pointer-constant -Wno-c++98-compat-pedantic
endif

# Makefile verbosity
ifeq ($(VERBOSE),1)
VERBOSE_MAKE=
else
VERBOSE_MAKE=@
endif

# Self tests parallelization
ifeq ($(OPENMP_SELF_TESTS),1)
CFLAGS  += -DWITH_OPENMP_SELF_TESTS -fopenmp
LDFLAGS += -fopenmp
endif
