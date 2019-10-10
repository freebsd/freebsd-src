########################################################
# Copyright 2019 ARM Limited. All rights reserved.
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

########
# RCTDL - test makefile for snapshot lister test.
#

CXX := $(MASTER_CXX)
LINKER := $(MASTER_LINKER)	

PROG = mem-buffer-eg

BUILD_DIR=./$(PLAT_DIR)

VPATH	=	 $(OCSD_TESTS)/source 

CXX_INCLUDES	=	\
			-I$(OCSD_TESTS)/source \
			-I$(OCSD_INCLUDE) \
			-I$(OCSD_TESTS)/snapshot_parser_lib/include

OBJECTS		=	$(BUILD_DIR)/mem_buff_demo.o

LIBS		=	-L$(LIB_TEST_TARGET_DIR) -lsnapshot_parser \
				-L$(LIB_TARGET_DIR) -l$(LIB_BASE_NAME)

all:  build_dir copy_libs

test_app: $(BIN_TEST_TARGET_DIR)/$(PROG)


 $(BIN_TEST_TARGET_DIR)/$(PROG): $(OBJECTS)
			mkdir -p  $(BIN_TEST_TARGET_DIR)
			$(LINKER) $(LDFLAGS) $(OBJECTS) -Wl,--start-group $(LIBS) -Wl,--end-group -o $(BIN_TEST_TARGET_DIR)/$(PROG)

build_dir:
	mkdir -p $(BUILD_DIR)

.PHONY: copy_libs
copy_libs: $(BIN_TEST_TARGET_DIR)/$(PROG)
	cp $(LIB_TARGET_DIR)/*.so* $(BIN_TEST_TARGET_DIR)/.



#### build rules
## object dependencies
DEPS := $(OBJECTS:%.o=%.d)

-include $(DEPS)

## object compile
$(BUILD_DIR)/%.o : %.cpp
			$(CXX) $(CXXFLAGS) $(CXX_INCLUDES) -MMD $< -o $@

#### clean
.PHONY: clean
clean :
	-rm $(BIN_TEST_TARGET_DIR)/$(PROG) $(OBJECTS)
	-rm $(DEPS)
	-rm $(BIN_TEST_TARGET_DIR)/*.so*
	-rmdir $(BUILD_DIR)

# end of file makefile
