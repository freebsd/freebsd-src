#/***********************license start***************
# Copyright (c) 2003-2007 Cavium Networks (support@cavium.com). All rights
# reserved.
#
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
#     * Neither the name of Cavium Networks nor the names of
#       its contributors may be used to endorse or promote products
#       derived from this software without specific prior written
#       permission.
#
# TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
# AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
# OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
# RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
# REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
# DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
# OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
# PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
# POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
# OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
#
#
# For any questions regarding licensing please contact marketing@caviumnetworks.com
#
# ***********************license end**************************************/

#
#  component Makefile fragment
#

#  standard component Makefile header
sp              :=  $(sp).x
dirstack_$(sp)  :=  $(d)
d               :=  $(dir)

#  component specification

LIBRARY := $(OBJ_DIR)/libcvmx.a

OBJS_$(d)  :=  \
	$(OBJ_DIR)/cvmx-bootmem.o \
	$(OBJ_DIR)/cvmx-cn3010-evb-hs5.o \
	$(OBJ_DIR)/cvmx-core.o \
	$(OBJ_DIR)/cvmx-coremask.o \
	$(OBJ_DIR)/cvmx-cmd-queue.o \
	$(OBJ_DIR)/cvmx-csr-db.o \
	$(OBJ_DIR)/cvmx-csr-db-support.o \
	$(OBJ_DIR)/cvmx-dfa.o \
	$(OBJ_DIR)/cvmx-dma-engine.o \
	$(OBJ_DIR)/cvmx-ebt3000.o \
	$(OBJ_DIR)/cvmx-flash.o \
	$(OBJ_DIR)/cvmx-fpa.o \
	$(OBJ_DIR)/cvmx-helper-board.o \
	$(OBJ_DIR)/cvmx-helper-errata.o \
	$(OBJ_DIR)/cvmx-helper-fpa.o \
	$(OBJ_DIR)/cvmx-helper-loop.o \
	$(OBJ_DIR)/cvmx-helper-npi.o \
	$(OBJ_DIR)/cvmx-helper-rgmii.o \
	$(OBJ_DIR)/cvmx-helper-sgmii.o \
	$(OBJ_DIR)/cvmx-helper-spi.o \
	$(OBJ_DIR)/cvmx-helper-util.o \
	$(OBJ_DIR)/cvmx-helper-xaui.o \
	$(OBJ_DIR)/cvmx-helper.o \
	$(OBJ_DIR)/cvmx-interrupt-rsl.o \
	$(OBJ_DIR)/cvmx-interrupt-decodes.o \
	$(OBJ_DIR)/cvmx-l2c.o \
	$(OBJ_DIR)/cvmx-llm.o \
	$(OBJ_DIR)/cvmx-log-arc.o \
	$(OBJ_DIR)/cvmx-log.o \
	$(OBJ_DIR)/cvmx-mgmt-port.o \
	$(OBJ_DIR)/cvmx-nand.o \
	$(OBJ_DIR)/cvmx-pcie.o \
	$(OBJ_DIR)/cvmx-pko.o \
	$(OBJ_DIR)/cvmx-pow.o \
	$(OBJ_DIR)/cvmx-raid.o \
	$(OBJ_DIR)/cvmx-spi.o \
	$(OBJ_DIR)/cvmx-spi4000.o \
	$(OBJ_DIR)/cvmx-sysinfo.o \
	$(OBJ_DIR)/cvmx-thunder.o \
	$(OBJ_DIR)/cvmx-tim.o \
	$(OBJ_DIR)/cvmx-tra.o \
	$(OBJ_DIR)/cvmx-twsi.o \
	$(OBJ_DIR)/cvmx-usb.o \
	$(OBJ_DIR)/cvmx-warn.o \
	$(OBJ_DIR)/cvmx-zip.o \
	$(OBJ_DIR)/cvmx-zone.o \
	$(OBJ_DIR)/octeon-model.o \
	$(OBJ_DIR)/octeon-pci-console.o
ifeq (linux,$(findstring linux,$(OCTEON_TARGET)))
OBJS_$(d)  +=  \
	$(OBJ_DIR)/cvmx-app-init-linux.o
else
OBJS_$(d)  +=  \
	$(OBJ_DIR)/cvmx-interrupt.o \
	$(OBJ_DIR)/cvmx-interrupt-handler.o \
	$(OBJ_DIR)/cvmx-app-init.o \
	$(OBJ_DIR)/cvmx-malloc.o
endif

$(OBJS_$(d)):  CFLAGS_LOCAL := -I$(d) -O2 -g -W -Wall -Wno-unused-parameter -Wundef

#  standard component Makefile rules

DEPS_$(d)   :=  $(OBJS_$(d):.o=.d)

LIBS_LIST   :=  $(LIBS_LIST) $(LIBRARY)

CLEAN_LIST  :=  $(CLEAN_LIST) $(OBJS_$(d)) $(DEPS_$(d)) $(LIBRARY)

-include $(DEPS_$(d))

$(LIBRARY): $(OBJS_$(d))
	$(AR) -cr $@ $^

$(OBJ_DIR)/%.o:	$(d)/%.c
	$(COMPILE)

$(OBJ_DIR)/%.o:	$(d)/%.S
	$(ASSEMBLE)

$(OBJ_DIR)/cvmx-app-init-linux.o: $(d)/cvmx-app-init-linux.c
	$(CC) $(CFLAGS_GLOBAL) $(CFLAGS_LOCAL) -MD -c -Umain -o $@ $<

CFLAGS_SPECIAL := -I$(d) -I$(d)/cvmx-malloc -O2 -g -DUSE_CVM_THREADS=1 -D_REENTRANT

$(OBJ_DIR)/cvmx-malloc.o: $(d)/cvmx-malloc/malloc.c
	$(CC) $(CFLAGS_GLOBAL) $(CFLAGS_SPECIAL) -MD -c -o $@ $<

#  standard component Makefile footer

d   :=  $(dirstack_$(sp))
sp  :=  $(basename $(sp))
