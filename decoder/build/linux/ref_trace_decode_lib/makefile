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
# OpenCSD - makefile for main trace decode library
#

CXX := $(MASTER_CXX)
LINKER := $(MASTER_LINKER)
LIB := $(MASTER_LIB)

LIB_NAME= lib$(LIB_BASE_NAME)

BUILD_DIR=./$(PLAT_DIR)

VPATH=	$(OCSD_SOURCE) \
		$(OCSD_SOURCE)/etmv3 \
		$(OCSD_SOURCE)/etmv4 \
		$(OCSD_SOURCE)/ptm \
		$(OCSD_SOURCE)/i_dec \
		$(OCSD_SOURCE)/mem_acc \
		$(OCSD_SOURCE)/stm \
		$(OCSD_SOURCE)/pkt_printers


CXX_INCLUDES= \
				-I$(OCSD_INCLUDE) \
				-I$(OCSD_SOURCE)

ETMV3OBJ=	$(BUILD_DIR)/trc_cmp_cfg_etmv3.o \
			$(BUILD_DIR)/trc_pkt_decode_etmv3.o \
			$(BUILD_DIR)/trc_pkt_elem_etmv3.o \
			$(BUILD_DIR)/trc_pkt_proc_etmv3.o \
			$(BUILD_DIR)/trc_pkt_proc_etmv3_impl.o

ETMV4OBJ=	$(BUILD_DIR)/trc_cmp_cfg_etmv4.o \
			$(BUILD_DIR)/trc_pkt_proc_etmv4i.o \
			$(BUILD_DIR)/trc_pkt_decode_etmv4i.o \
			$(BUILD_DIR)/trc_pkt_elem_etmv4i.o \
			$(BUILD_DIR)/trc_etmv4_stack_elem.o

PTMOBJ=		$(BUILD_DIR)/trc_cmp_cfg_ptm.o \
			$(BUILD_DIR)/trc_pkt_elem_ptm.o \
			$(BUILD_DIR)/trc_pkt_proc_ptm.o \
			$(BUILD_DIR)/trc_pkt_decode_ptm.o

IDECOBJ=	$(BUILD_DIR)/trc_i_decode.o \
			$(BUILD_DIR)/trc_idec_arminst.o

MEMACCOBJ=	$(BUILD_DIR)/trc_mem_acc_mapper.o \
			$(BUILD_DIR)/trc_mem_acc_bufptr.o \
			$(BUILD_DIR)/trc_mem_acc_file.o \
			$(BUILD_DIR)/trc_mem_acc_base.o \
			$(BUILD_DIR)/trc_mem_acc_cb.o \
			$(BUILD_DIR)/trc_mem_acc_cache.o

STMOBJ=		$(BUILD_DIR)/trc_pkt_elem_stm.o \
			$(BUILD_DIR)/trc_pkt_proc_stm.o \
			$(BUILD_DIR)/trc_pkt_decode_stm.o

PKTPRNTOBJ=	$(BUILD_DIR)/raw_frame_printer.o \
			$(BUILD_DIR)/trc_print_fact.o


OBJECTS=$(BUILD_DIR)/ocsd_code_follower.o \
		$(BUILD_DIR)/ocsd_dcd_tree.o \
		$(BUILD_DIR)/ocsd_error.o \
		$(BUILD_DIR)/ocsd_error_logger.o \
		$(BUILD_DIR)/ocsd_gen_elem_list.o \
		$(BUILD_DIR)/ocsd_gen_elem_stack.o \
		$(BUILD_DIR)/ocsd_lib_dcd_register.o \
		$(BUILD_DIR)/ocsd_msg_logger.o \
		$(BUILD_DIR)/ocsd_version.o \
		$(BUILD_DIR)/trc_component.o \
		$(BUILD_DIR)/trc_core_arch_map.o \
		$(BUILD_DIR)/trc_frame_deformatter.o \
		$(BUILD_DIR)/trc_gen_elem.o \
		$(BUILD_DIR)/trc_printable_elem.o \
		$(BUILD_DIR)/trc_ret_stack.o \
		$(ETMV3OBJ) \
		$(ETMV4OBJ) \
		$(IDECOBJ) \
		$(MEMACCOBJ) \
		$(STMOBJ) \
		$(PTMOBJ) \
		$(PKTPRNTOBJ)

all: links

links: $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_MAJOR_VER) $(LIB_TARGET_DIR)/$(LIB_NAME).so
.PHONY: links

LIBS:= $(LIB_TARGET_DIR)/$(LIB_NAME).a $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_VER)

$(LIB_TARGET_DIR):
	mkdir -p $(LIB_TARGET_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_TARGET_DIR)/$(LIB_NAME).a: $(OBJECTS) | $(BUILD_DIR) $(LIB_TARGET_DIR)
	$(LIB) $(ARFLAGS) $(LIB_TARGET_DIR)/$(LIB_NAME).a $(OBJECTS)

$(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_VER): $(OBJECTS) | $(BUILD_DIR) $(LIB_TARGET_DIR)
	$(LINKER) $(LDFLAGS) -shared -o $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_VER) -Wl,-soname,$(LIB_NAME).so.$(SO_MAJOR_VER) $(OBJECTS)

$(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_MAJOR_VER): $(LIBS) | $(LIB_TARGET_DIR)
	( cd $(LIB_TARGET_DIR);  ln -sf $(LIB_NAME).so.$(SO_VER) $(LIB_NAME).so.$(SO_MAJOR_VER) )

$(LIB_TARGET_DIR)/$(LIB_NAME).so: $(LIB_TARGET_DIR)/$(LIB_NAME).so.$(SO_MAJOR_VER) | $(LIB_TARGET_DIR)
	( cd $(LIB_TARGET_DIR);  ln -sf $(LIB_NAME).so.$(SO_MAJOR_VER) $(LIB_NAME).so )


##### build rules

## object dependencies
DEPS := $(OBJECTS:%.o=%.d)

-include $(DEPS)

## object compile
$(BUILD_DIR)/%.o : %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CXX_INCLUDES) -MMD  $< -o $@


#### clean
.PHONY: clean
clean:
	rm -f $(OBJECTS)
	rm -f $(DEPS)
	rm -f $(LIB_TARGET_DIR)/$(LIB_NAME).a
	rm -f $(LIB_TARGET_DIR)/$(LIB_NAME).so*
	-rmdir $(BUILD_DIR)
