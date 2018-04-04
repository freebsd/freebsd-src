########################################################
# Copyright 2015 ARM Limited. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
# 
# 3. Neither the name of the copyright holder nor the names of its contributors 
# may be used to endorse or promote products derived from this software without 
# specific prior written permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
# 
#################################################################################
# OpenCSD - master makefile for libraries and tests
#
# command line options
# DEBUG=1 	    create a debug build
#

# Set project root - relative to build directory
ifeq ($(OCSD_ROOT),)
OCSD_ROOT := $(shell pwd | sed 's,/build/linux.*,,')
export OCSD_ROOT
endif

# library names
LIB_BASE_NAME=opencsd
export LIB_BASE_NAME
LIB_CAPI_NAME=$(LIB_BASE_NAME)_c_api
export LIB_CAPI_NAME

# determine base architecture, heavily borrowed from the Linux kernel v4.4's
# tools/perf/config/Makefile.arch
# For example, to compile for arm64 on a X86 PC, you can issue the command:
# "export ARCH=arm64"
ifndef ARCH
ARCH := $(shell uname -m 2>/dev/null || echo not)
endif

# source root directories
export OCSD_LIB_ROOT=$(OCSD_ROOT)/lib

export OCSD_INCLUDE=$(OCSD_ROOT)/include
export OCSD_SOURCE=$(OCSD_ROOT)/source

export OCSD_TESTS=$(OCSD_ROOT)/tests

export LIB_UAPI_INC_DIR=opencsd

# tools
export MASTER_CC=$(CROSS_COMPILE)gcc
export MASTER_CPP=$(CROSS_COMPILE)g++
export MASTER_LINKER=$(CROSS_COMPILE)g++
export MASTER_LIB=$(CROSS_COMPILE)ar
export INSTALL=install

# installation directory
INSTALL_LIB_DIR=/usr/lib/
export INSTALL_INCLUDE_DIR=/usr/include/

# compile flags
MASTER_CC_FLAGS := -c -Wall -DLINUX
MASTER_CPP_FLAGS := -c -Wall -DLINUX -std=c++11
MASTER_LINKER_FLAGS := -Wl,-z,defs
MASTER_LIB_FLAGS := rcs

# debug variant
ifdef DEBUG
MASTER_CC_FLAGS += -g -O0 -DDEBUG
MASTER_CPP_FLAGS += -g -O0 -DDEBUG
BUILD_VARIANT=dbg
else
MASTER_CC_FLAGS += -g -O2 -DNDEBUG
MASTER_CPP_FLAGS += -g -O2 -DNDEBUG
BUILD_VARIANT=rel
endif


# platform bit size variant
ifeq ($(ARCH),x86)
   MFLAG:="-m32"
   BIT_VARIANT=32
else ifeq ($(ARCH),x86_64)
   MFLAG:="-m64"
   BIT_VARIANT=64
else ifeq ($(ARCH),arm)
   BIT_VARIANT=-arm
else ifeq ($(ARCH),arm64)
   BIT_VARIANT=-arm64
else ifeq ($(ARCH),aarch64)
   BIT_VARIANT=-arm64
else ifeq ($(ARCH),aarch32)
   BIT_VARIANT=-arm
endif

MASTER_CC_FLAGS += $(MFLAG)
MASTER_CPP_FLAGS += $(MFLAG)
MASTER_LINKER_FLAGS += $(MFLAG)

# export build flags
export MASTER_CC_FLAGS
export MASTER_CPP_FLAGS
export MASTER_LINKER_FLAGS
export MASTER_LIB_FLAGS

# target directories
export PLAT_DIR=linux$(BIT_VARIANT)/$(BUILD_VARIANT)
export LIB_TARGET_DIR=$(OCSD_LIB_ROOT)/$(PLAT_DIR)
export LIB_TEST_TARGET_DIR=$(OCSD_TESTS)/lib/$(PLAT_DIR)
export BIN_TEST_TARGET_DIR=$(OCSD_TESTS)/bin/$(PLAT_DIR)

###########################################################
# build targets

all: libs tests

libs: $(LIB_BASE_NAME)_lib  $(LIB_CAPI_NAME)_lib

install: libs
	$(INSTALL) --mode=644 $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so $(INSTALL_LIB_DIR)/
	$(INSTALL) --mode=644 $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so $(INSTALL_LIB_DIR)/
	cd $(OCSD_ROOT)/build/linux/rctdl_c_api_lib && make install_inc

################################
# build  OpenCSD trace decode library
#
$(LIB_BASE_NAME)_lib: $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).a $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so

$(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so: $(LIB_BASE_NAME)_all
$(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).a: $(LIB_BASE_NAME)_all

# single command builds both .a and .so targets in sub-makefile
$(LIB_BASE_NAME)_all:
	mkdir -p $(LIB_TARGET_DIR)
	cd $(OCSD_ROOT)/build/linux/ref_trace_decode_lib && make

################################
# build OpenCSD trace decode C API library 
#
$(LIB_CAPI_NAME)_lib: $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).a $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so

$(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so: $(LIB_CAPI_NAME)_all
$(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).a:  $(LIB_CAPI_NAME)_all

# single command builds both .a and .so targets in sub-makefile
$(LIB_CAPI_NAME)_all:  $(LIB_BASE_NAME)_lib
	mkdir -p $(LIB_TARGET_DIR)
	cd $(OCSD_ROOT)/build/linux/rctdl_c_api_lib && make

#################################
# build tests

.PHONY: tests
tests: libs
	cd $(OCSD_ROOT)/tests/build/linux/echo_test_dcd_lib && make
	cd $(OCSD_ROOT)/tests/build/linux/snapshot_parser_lib && make
	cd $(OCSD_ROOT)/tests/build/linux/trc_pkt_lister && make
	cd $(OCSD_ROOT)/tests/build/linux/c_api_pkt_print_test && make

#############################################################
# clean targets
#
clean: clean_libs clean_tests

.PHONY: clean_libs clean_tests

clean_libs:
	cd $(OCSD_ROOT)/build/linux/ref_trace_decode_lib && make clean
	cd $(OCSD_ROOT)/build/linux/rctdl_c_api_lib && make clean

clean_tests:
	cd $(OCSD_ROOT)/tests/build/linux/echo_test_dcd_lib && make clean
	cd $(OCSD_ROOT)/tests/build/linux/snapshot_parser_lib && make clean
	cd $(OCSD_ROOT)/tests/build/linux/trc_pkt_lister && make clean
	cd $(OCSD_ROOT)/tests/build/linux/c_api_pkt_print_test && make clean

clean_install:
	rm -f $(INSTALL_LIB_DIR)/lib$(LIB_BASE_NAME).so
	rm -f $(INSTALL_LIB_DIR)/lib$(LIB_CAPI_NAME).so
	rm -rf $(INSTALL_INCLUDE_DIR)/$(LIB_UAPI_INC_DIR)
