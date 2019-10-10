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
# OpenCSD - makefile for C API wrapper library
#

CXX := $(MASTER_CXX)
LINKER := $(MASTER_LINKER)
LIB := $(MASTER_LIB)

LIB_NAME = lib$(LIB_CAPI_NAME)
SO_LIB_DEPS= -L$(LIB_TARGET_DIR) -l$(LIB_BASE_NAME)

BUILD_DIR=./$(PLAT_DIR)

VPATH=	$(OCSD_SOURCE)/c_api

CXX_INCLUDES= \
				-I$(OCSD_INCLUDE) \
				-I$(OCSD_SOURCE)/c_api

OBJECTS=$(BUILD_DIR)/ocsd_c_api.o \
		$(BUILD_DIR)/ocsd_c_api_custom_obj.o

INST_INC_SRC=$(OCSD_INCLUDE)/$(LIB_UAPI_INC_DIR)
INST_INC_DST=$(INSTALL_INCLUDE_DIR)/$(LIB_UAPI_INC_DIR)

all: links

links: $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_MAJOR_VER) $(LIB_TARGET_DIR)/$(LIB_NAME).so
.PHONY: links

LIBS:= $(LIB_TARGET_DIR)/$(LIB_NAME).a $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_VER)

$(LIB_TARGET_DIR):
	mkdir -p  $(LIB_TARGET_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_TARGET_DIR)/$(LIB_NAME).a: $(OBJECTS) | $(BUILD_DIR) $(LIB_TARGET_DIR)
	$(LIB) $(ARFLAGS) $(LIB_TARGET_DIR)/$(LIB_NAME).a $(OBJECTS)

$(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_VER): $(OBJECTS) | $(BUILD_DIR) $(LIB_TARGET_DIR)
	$(LINKER) $(LDFLAGS) -shared -o $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_VER) -Wl,-soname,$(LIB_NAME).so.$(SO_MAJOR_VER) $(OBJECTS) $(SO_LIB_DEPS)

$(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_MAJOR_VER): $(LIBS) | $(LIB_TARGET_DIR)
	( cd $(LIB_TARGET_DIR); ln -sf $(LIB_NAME).so.$(SO_VER) $(LIB_NAME).so.$(SO_MAJOR_VER) )

$(LIB_TARGET_DIR)/$(LIB_NAME).so: $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_MAJOR_VER) | $(LIB_TARGET_DIR)
	( cd $(LIB_TARGET_DIR); ln -sf $(LIB_NAME).so.$(SO_MAJOR_VER) $(LIB_NAME).so )


##### build rules

## object dependencies
DEPS := $(OBJECTS:%.o=%.d)

-include $(DEPS)

## object compile
$(BUILD_DIR)/%.o : %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDES) -MMD $< -o $@


#### clean
.PHONY: clean
clean:
	rm -f $(OBJECTS)
	rm -f $(DEPS)
	rm -f $(LIB_TARGET_DIR)/$(LIB_NAME).a
	rm -f $(LIB_TARGET_DIR)/$(LIB_NAME).so*
	-rmdir $(BUILD_DIR)

#### install the necessary include files for the c-api library on linux 
install_inc:
	$(INSTALL) -d --mode=0755 $(INST_INC_DST)/
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/trc_gen_elem_types.h $(INST_INC_DST)/
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/ocsd_if_types.h $(INST_INC_DST)/
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/ocsd_if_version.h $(INST_INC_DST)/
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/trc_pkt_types.h $(INST_INC_DST)/
	$(INSTALL) -d --mode=0755 $(INST_INC_DST)/ptm
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/ptm/trc_pkt_types_ptm.h $(INST_INC_DST)/ptm/
	$(INSTALL) -d --mode=0755 $(INST_INC_DST)/stm
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/stm/trc_pkt_types_stm.h $(INST_INC_DST)/stm/
	$(INSTALL) -d --mode=0755 $(INST_INC_DST)/etmv3
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/etmv3/trc_pkt_types_etmv3.h $(INST_INC_DST)/etmv3/
	$(INSTALL) -d --mode=0755 $(INST_INC_DST)/etmv4
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/etmv4/trc_pkt_types_etmv4.h $(INST_INC_DST)/etmv4/
	$(INSTALL) -d --mode=0755 $(INST_INC_DST)/c_api
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/c_api/ocsd_c_api_types.h $(INST_INC_DST)/c_api/
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/c_api/opencsd_c_api.h $(INST_INC_DST)/c_api/
	$(INSTALL) --mode=0644 $(INST_INC_SRC)/c_api/ocsd_c_api_custom.h $(INST_INC_DST)/c_api/


