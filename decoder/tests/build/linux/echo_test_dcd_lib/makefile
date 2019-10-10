########################################################
# Copyright 2016 ARM Limited. All rights reserved.
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
# OpenCSD - makefile for external echo_test decoder.
#

CC := $(MASTER_CC)
LIB := $(MASTER_LIB)

LIB_NAME = lib_echo_test_dcd

BUILD_DIR=./$(PLAT_DIR)

ECHO_TEST_PATH=$(OCSD_TESTS)/ext_dcd_test_eg/c_api_echo_test

VPATH	=	 $(ECHO_TEST_PATH)

CC_INCLUDES	=	\
			-I$(ECHO_TEST_PATH) \
			-I$(OCSD_INCLUDE) 

OBJECTS		=	$(BUILD_DIR)/ext_dcd_echo_test.o \
				$(BUILD_DIR)/ext_dcd_echo_test_fact.o

all:  build_dir $(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a

$(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a: $(OBJECTS)
	mkdir -p $(LIB_TEST_TARGET_DIR)
	$(LIB) $(ARFLAGS) $(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a $(OBJECTS)

build_dir:
	mkdir -p $(BUILD_DIR)

#### build rules
## object dependencies
DEPS := $(OBJECTS:%.o=%.d)

-include $(DEPS)

## object compile
$(BUILD_DIR)/%.o : %.c
			$(CC) $(CFLAGS) $(CC_INCLUDES) -MMD $< -o $@ 

#### clean
.PHONY: clean
clean:
	-rm $(OBJECTS)
	-rm $(LIB_TEST_TARGET_DIR)/$(LIB_NAME).a
	-rm $(DEPS)
	-rmdir $(BUILD_DIR) $(LIB_TEST_TARGET_DIR)

# end of file makefile
