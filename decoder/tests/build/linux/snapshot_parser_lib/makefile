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
########################################################################
#
# Make file for snapshot parser library.
#
########################################################################

CXX := $(MASTER_CXX)
LINKER := $(MASTER_LINKER)
LIB := $(MASTER_LIB)

# avoid build warnings in donated test code
WSUPPRESS= -Wno-deprecated-declarations -Wno-unused-variable -Wno-reorder

CXXFLAGS += $(WSUPPRESS)

LIB_NAME = libsnapshot_parser

BUILD_DIR=./$(PLAT_DIR)

PARSER_ROOT=$(OCSD_TESTS)/snapshot_parser_lib
PARSER_SOURCE=$(PARSER_ROOT)/source
PARSER_INCLUDE=$(PARSER_ROOT)/include

VPATH=	$(PARSER_SOURCE)


CXX_INCLUDES= \
				-I$(PARSER_INCLUDE) \
				-I$(OCSD_INCLUDE)

OBJECTS=$(BUILD_DIR)/device_info.o \
		$(BUILD_DIR)/device_parser.o \
		$(BUILD_DIR)/snapshot_parser.o \
		$(BUILD_DIR)/snapshot_parser_util.o \
		$(BUILD_DIR)/snapshot_reader.o \
		$(BUILD_DIR)/ss_to_dcdtree.o

all: build_dir $(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a

$(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a: $(OBJECTS)
	mkdir -p $(LIB_TEST_TARGET_DIR)
	$(LIB) $(ARFLAGS) $(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a $(OBJECTS)

build_dir:
	mkdir -p $(BUILD_DIR)

##### build rules

## object dependencies
DEPS := $(OBJECTS:%.o=%.d)

-include $(DEPS)

## object compile
$(BUILD_DIR)/%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDES) -MMD $< -o $@

### clean
.PHONY: clean
clean:
	-rm $(OBJECTS)
	-rm $(DEPS)
	-rm $(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a
	-rmdir $(BUILD_DIR) $(LIB_TEST_TARGET_DIR)
