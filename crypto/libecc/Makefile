.SUFFIXES:

# Where to put generated objects
BUILD_DIR ?= build
# Default to the previous behaviour and keep generated .o & .d files next to the source code
OBJ_DIR ?=.
include common.mk


# Static libraries to build
LIBS = $(LIBARITH) $(LIBEC) $(LIBSIGN)

# Compile dynamic libraries if the user asked to
ifeq ($(WITH_DYNAMIC_LIBS),1)
LIBS += $(LIBARITH_DYN) $(LIBEC_DYN) $(LIBSIGN_DYN)
endif

# Executables to build
TESTS_EXEC = $(BUILD_DIR)/ec_self_tests $(BUILD_DIR)/ec_utils
# We also compile executables with dynamic linking if asked to
ifeq ($(WITH_DYNAMIC_LIBS),1)
TESTS_EXEC += $(BUILD_DIR)/ec_self_tests_dyn $(BUILD_DIR)/ec_utils_dyn
endif

EXEC_TO_CLEAN = $(BUILD_DIR)/ec_self_tests $(BUILD_DIR)/ec_utils $(BUILD_DIR)/ec_self_tests_dyn $(BUILD_DIR)/ec_utils_dyn

# all and clean, as you might expect
all: $(LIBS) $(TESTS_EXEC)

# Default object files extension
OBJ_FILES_EXTENSION ?= o

clean:
	@rm -f $(LIBS) $(EXEC_TO_CLEAN)
	@find $(OBJ_DIR)/ -name '*.$(OBJ_FILES_EXTENSION)' -exec rm -f '{}' \;
	@find $(OBJ_DIR)/ -name '*.d' -exec rm -f '{}' \;
	@find $(BUILD_DIR)/ -name '*.a' -exec rm -f '{}' \;
	@find $(BUILD_DIR)/ -name '*.so' -exec rm -f '{}' \;
	@find . -name '*~'  -exec rm -f '{}' \;



# --- Source Code ---

# external dependencies
EXT_DEPS_SRC = $(wildcard src/external_deps/*.c)

# utils module (for the ARITH layer, we only need
# NN and FP - and not curves - related stuff. Same goes
# for EC and SIGN. Hence the distinction between three
# sets of utils objects.
UTILS_ARITH_SRC = src/utils/utils.c src/utils/utils_rand.c
UTILS_ARITH_SRC += $(wildcard src/utils/*_nn.c)
UTILS_ARITH_SRC += $(wildcard src/utils/*_fp.c)
UTILS_ARITH_SRC += $(wildcard src/utils/*_buf.c)
UTILS_EC_SRC = $(wildcard src/utils/*_curves.c)
UTILS_SIGN_SRC = $(wildcard src/utils/*_keys.c)

# nn module
NN_SRC = $(wildcard src/nn/n*.c)

# fp module
FP_SRC = $(wildcard src/fp/fp*.c)

# curve module
CURVES_SRC = $(wildcard src/curves/*.c)

# Hash module
HASH_SRC = $(wildcard src/hash/sha*.c) $(wildcard src/hash/bash*.c) src/hash/hash_algs.c src/hash/sm3.c src/hash/streebog.c src/hash/ripemd160.c src/hash/belt-hash.c src/hash/hmac.c

# Key/Signature/Verification/ECDH module
SIG_SRC = $(wildcard src/sig/*dsa.c) src/sig/ecdsa_common.c src/sig/ecsdsa_common.c src/sig/sig_algs.c src/sig/sm2.c src/sig/bign_common.c src/sig/bign.c src/sig/dbign.c src/sig/bip0340.c
ECDH_SRC = $(wildcard src/ecdh/*.c)
KEY_SRC = src/sig/ec_key.c

# Test elements
TESTS_OBJECTS_CORE_SRC = src/tests/ec_self_tests_core.c
TESTS_OBJECTS_SELF_SRC = src/tests/ec_self_tests.c
TESTS_OBJECTS_UTILS_SRC = src/tests/ec_utils.c



# --- Static Libraries ---

LIBARITH_SRC = $(FP_SRC) $(NN_SRC) $(UTILS_ARITH_SRC)
LIBARITH_OBJECTS = $(patsubst %,$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION),$(basename $(LIBARITH_SRC)))
$(LIBARITH): $(LIBARITH_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(AR) $(AR_FLAGS) $@ $^
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(RANLIB) $(RANLIB_FLAGS) $@

LIBEC_SRC = $(LIBARITH_SRC) $(CURVES_SRC) $(UTILS_EC_SRC)
LIBEC_OBJECTS = $(patsubst %,$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION),$(basename $(LIBEC_SRC)))
$(LIBEC): $(LIBEC_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(AR) $(AR_FLAGS) $@ $^
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(RANLIB) $(RANLIB_FLAGS) $@

LIBSIGN_SRC = $(LIBEC_SRC) $(HASH_SRC) $(SIG_SRC) $(KEY_SRC) $(UTILS_SIGN_SRC) $(ECDH_SRC)
LIBSIGN_OBJECTS = $(patsubst %,$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION),$(basename $(LIBSIGN_SRC)))
$(LIBSIGN): $(LIBSIGN_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(AR) $(AR_FLAGS) $@ $^
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(RANLIB) $(RANLIB_FLAGS) $@



# --- Dynamic Libraries ---

$(LIBARITH_DYN): $(LIBARITH_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(LIB_CFLAGS) $(LIB_DYN_LDFLAGS) $^ -o $@

$(LIBEC_DYN): $(LIBEC_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(LIB_CFLAGS) $(LIB_DYN_LDFLAGS) $^ -o $@

$(LIBSIGN_DYN): $(LIBSIGN_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(LIB_CFLAGS) $(LIB_DYN_LDFLAGS) $^ -o $@



# --- Executables (Static linkage with libsign object files) ---

EC_SELF_TESTS_SRC = $(TESTS_OBJECTS_CORE_SRC) $(TESTS_OBJECTS_SELF_SRC) $(EXT_DEPS_SRC)
EC_SELF_TESTS_OBJECTS = $(patsubst %,$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION),$(basename $(EC_SELF_TESTS_SRC)))
$(BUILD_DIR)/ec_self_tests: $(EC_SELF_TESTS_OBJECTS) $(LIBSIGN_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) $(BIN_LDFLAGS) $^ -o $@

EC_UTILS_SRC = $(TESTS_OBJECTS_CORE_SRC) $(TESTS_OBJECTS_UTILS_SRC) $(EXT_DEPS_SRC)
EC_UTILS_OBJECTS = $(patsubst %,$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION),$(basename $(EC_UTILS_SRC)))
$(BUILD_DIR)/ec_utils: $(EC_UTILS_SRC) $(LIBSIGN_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) $(BIN_LDFLAGS) -DWITH_STDLIB  $^ -o $@



# --- Excutables (Dynamic linkage with libsign shared library) ---

$(BUILD_DIR)/ec_self_tests_dyn: $(EC_SELF_TESTS_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) $(BIN_LDFLAGS) -L$(BUILD_DIR) $^ -lsign -o $@

$(BUILD_DIR)/ec_utils_dyn: $(EC_UTILS_OBJECTS)
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) $(BIN_CFLAGS) $(BIN_LDFLAGS) -L$(BUILD_DIR) -DWITH_STDLIB  $^ -lsign -o $@



.PHONY: all clean 16 32 64 debug debug16 debug32 debug64 force_arch32 force_arch64

# All source files, used to construct general rules
SRC += $(EXT_DEPS_SRC) $(UTILS_ARITH_SRC) $(UTILS_EC_SRC) $(UTILS_SIGN_SRC)
SRC += $(NN_SRC) $(FP_SRC) $(CURVES_SRC) $(HASH_SRC) $(SIG_SRC) $(ECDH_SRC)
SRC += $(KEY_SRC) $(TESTS_OBJECTS_CORE_SRC) $(TESTS_OBJECTS_SELF_SRC)
SRC += $(TESTS_OBJECTS_UTILS_SRC)

# All object files
OBJS = $(patsubst %,$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION),$(basename $(SRC)))

# General dependency rule between .o and .d files
DEPS = $(OBJS:.$(OBJ_FILES_EXTENSION)=.d)

# General rule for creating .o (and .d) file from .c
$(OBJ_DIR)/%.$(OBJ_FILES_EXTENSION): %.c
	$(VERBOSE_MAKE)$(CROSS_COMPILE)$(CC) -c $(LIB_CFLAGS) -o $@ $<

# Populate the directory structure to contain the .o and .d files, if necessary
$(shell mkdir -p $(dir $(OBJS)) >/dev/null)
$(shell mkdir -p $(BUILD_DIR) >/dev/null)

# Make a note of the MAKEFILE_LIST at this stage of parsing the Makefile
# It is important here to use the ':=' operator so it is evaluated only once,
# and to do this before all the DEPS files are included as makefiles.
MAKEFILES:=$(MAKEFILE_LIST)

# Make object files depend on all makefiles used - this forces a rebuild if any
# of the makefiles are changed
$(OBJS) : $(MAKEFILES)

# Dep files are makefiles that keep track of which header files are used by the
# c source code. Include them to allow them to work correctly.
-include $(DEPS)
