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

# Set project root - relative to build makefile
ifeq ($(OCSD_ROOT),)
OCSD_ROOT := $(shell echo $(dir $(abspath $(lastword $(MAKEFILE_LIST)))) | sed 's,/build/linux.*,,')
export OCSD_ROOT
endif

# library names
LIB_BASE_NAME=opencsd
export LIB_BASE_NAME
LIB_CAPI_NAME=$(LIB_BASE_NAME)_c_api
export LIB_CAPI_NAME

# source root directories
export OCSD_LIB_ROOT=$(OCSD_ROOT)/lib

export OCSD_INCLUDE=$(OCSD_ROOT)/include
export OCSD_SOURCE=$(OCSD_ROOT)/source

export OCSD_TESTS=$(OCSD_ROOT)/tests
export LIB_UAPI_INC_DIR=opencsd

# tools
export MASTER_CC=$(CROSS_COMPILE)gcc
export MASTER_CXX=$(CROSS_COMPILE)g++
export MASTER_LINKER=$(CROSS_COMPILE)g++
export MASTER_LIB=$(CROSS_COMPILE)ar
export INSTALL=install


# installation directory
PREFIX ?=/usr
LIB_PATH ?= lib
INSTALL_LIB_DIR=$(PREFIX)/$(LIB_PATH)
INSTALL_BIN_DIR=$(PREFIX)/bin
export INSTALL_INCLUDE_DIR=$(PREFIX)/include/

# compile flags
CFLAGS += $(CPPFLAGS) -c -Wall -DLINUX -Wno-switch -fPIC
CXXFLAGS += $(CPPFLAGS) -c -Wall -DLINUX -Wno-switch -fPIC -std=c++11
LDFLAGS += -Wl,-z,defs
ARFLAGS ?= rcs

# debug variant
ifdef DEBUG
CFLAGS += -g -O0 -DDEBUG
CXXFLAGS += -g -O0 -DDEBUG
BUILD_VARIANT=dbg
else
CFLAGS += -g -O2 -DNDEBUG
CXXFLAGS += -g -O2 -DNDEBUG
BUILD_VARIANT=rel
endif

# export build flags
export CFLAGS
export CXXFLAGS
export LDFLAGS
export ARFLAGS

# target directories - fixed for default packaging build
PLAT_DIR ?= builddir
export PLAT_DIR
export LIB_TARGET_DIR=$(OCSD_LIB_ROOT)/$(PLAT_DIR)
export LIB_TEST_TARGET_DIR=$(OCSD_TESTS)/lib/$(PLAT_DIR)
export BIN_TEST_TARGET_DIR=$(OCSD_TESTS)/bin/$(PLAT_DIR)

# Fish version out of header file (converting from hex)
getver:=printf "%d" $$(awk '/\#define VARNAME/ { print $$3 }' $(OCSD_ROOT)/include/opencsd/ocsd_if_version.h)
export SO_MAJOR_VER := $(shell $(subst VARNAME,OCSD_VER_MAJOR,$(getver)))
SO_MINOR_VER := $(shell $(subst VARNAME,OCSD_VER_MINOR,$(getver)))
SO_PATCH_VER := $(shell $(subst VARNAME,OCSD_VER_PATCH,$(getver)))
export SO_VER := $(SO_MAJOR_VER).$(SO_MINOR_VER).$(SO_PATCH_VER)


###########################################################
# build targets

all: libs tests

libs: $(LIB_BASE_NAME)_lib  $(LIB_CAPI_NAME)_lib

install: libs tests
	mkdir -p $(INSTALL_LIB_DIR) $(INSTALL_INCLUDE_DIR) $(INSTALL_BIN_DIR)
	cp -d $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so $(INSTALL_LIB_DIR)/
	cp -d $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so.$(SO_MAJOR_VER) $(INSTALL_LIB_DIR)/
	$(INSTALL) --mode=644 $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so.$(SO_VER) $(INSTALL_LIB_DIR)/
	cp -d $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so $(INSTALL_LIB_DIR)/
	cp -d $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so.$(SO_MAJOR_VER) $(INSTALL_LIB_DIR)/
	$(INSTALL) --mode=644 $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so.$(SO_VER) $(INSTALL_LIB_DIR)/
	$(INSTALL) --mode=644 $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).a $(INSTALL_LIB_DIR)/
	$(INSTALL) --mode=644 $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).a $(INSTALL_LIB_DIR)/
	cd $(OCSD_ROOT)/build/linux/rctdl_c_api_lib && make install_inc
	$(INSTALL) --mode=755 $(BIN_TEST_TARGET_DIR)/trc_pkt_lister $(INSTALL_BIN_DIR)/ 


################################
# build  OpenCSD trace decode library
#
$(LIB_BASE_NAME)_lib: $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).a $(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so

$(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).so: $(LIB_BASE_NAME)_all
$(LIB_TARGET_DIR)/lib$(LIB_BASE_NAME).a: $(LIB_BASE_NAME)_all

# single command builds both .a and .so targets in sub-makefile
$(LIB_BASE_NAME)_all:
	mkdir -p $(LIB_TARGET_DIR)
	cd $(OCSD_ROOT)/build/linux/ref_trace_decode_lib && $(MAKE)

################################
# build OpenCSD trace decode C API library 
#
$(LIB_CAPI_NAME)_lib: $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).a $(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so

$(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).so: $(LIB_CAPI_NAME)_all
$(LIB_TARGET_DIR)/lib$(LIB_CAPI_NAME).a:  $(LIB_CAPI_NAME)_all

# single command builds both .a and .so targets in sub-makefile
$(LIB_CAPI_NAME)_all:  $(LIB_BASE_NAME)_lib
	mkdir -p $(LIB_TARGET_DIR)
	cd $(OCSD_ROOT)/build/linux/rctdl_c_api_lib && $(MAKE)

#################################
# build tests

.PHONY: tests
tests: libs
	cd $(OCSD_ROOT)/tests/build/linux/echo_test_dcd_lib && $(MAKE)
	cd $(OCSD_ROOT)/tests/build/linux/snapshot_parser_lib && $(MAKE)
	cd $(OCSD_ROOT)/tests/build/linux/trc_pkt_lister && $(MAKE)
	cd $(OCSD_ROOT)/tests/build/linux/c_api_pkt_print_test && $(MAKE)
	cd $(OCSD_ROOT)/tests/build/linux/mem_buffer_eg && $(MAKE)

#
# build docs
.PHONY: docs
docs:
	(cd $(OCSD_ROOT)/docs; doxygen doxygen_config.dox)


#############################################################
# clean targets
#
clean: clean_libs clean_tests clean_docs

.PHONY: clean_libs clean_tests clean_docs clean_install

clean_libs:
	cd $(OCSD_ROOT)/build/linux/ref_trace_decode_lib && $(MAKE) clean
	cd $(OCSD_ROOT)/build/linux/rctdl_c_api_lib && $(MAKE) clean

clean_tests:
	cd $(OCSD_ROOT)/tests/build/linux/echo_test_dcd_lib && $(MAKE) clean
	cd $(OCSD_ROOT)/tests/build/linux/snapshot_parser_lib && $(MAKE) clean
	cd $(OCSD_ROOT)/tests/build/linux/trc_pkt_lister && $(MAKE) clean
	cd $(OCSD_ROOT)/tests/build/linux/c_api_pkt_print_test && $(MAKE) clean
	cd $(OCSD_ROOT)/tests/build/linux/mem_buffer_eg && $(MAKE) clean
	-rmdir $(OCSD_TESTS)/lib

clean_docs:
	-rm -r $(OCSD_ROOT)/docs/html

clean_install:
	-rm    $(INSTALL_LIB_DIR)/lib$(LIB_BASE_NAME).*
	-rm    $(INSTALL_LIB_DIR)/lib$(LIB_CAPI_NAME).*
	-rm -r $(INSTALL_INCLUDE_DIR)/$(LIB_UAPI_INC_DIR)
