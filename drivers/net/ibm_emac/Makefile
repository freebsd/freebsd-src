#
# Makefile for the On-chip  ethernet drivers
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...


obj-y   := 
obj-m   := 

O_TARGET := ocp.o

# Module support doesn't work yet.
export-objs     := 

obj-$(CONFIG_IBM_OCP_ENET) += ibm_ocp_enet.o ibm_ocp_phy.o ibm_ocp_mal.o

# Only need this if you want to see additional debug messages
ifeq ($(CONFIG_IBM_OCP_ENET_ERROR_MSG), y)
obj-$(CONFIG_IBM_OCP_ENET) += ibm_ocp_debug.o
endif

include $(TOPDIR)/Rules.make
