SCST_INC_DIR := $(SUBDIRS)/../../scst/include/

EXTRA_CFLAGS +=  -I$(SCST_INC_DIR)
EXTRA_CFLAGS +=  -DRHEL_RELEASE_CODE
#EXTRA_CFLAGS += -DCONFIG_SCST_DEBUG -g
#EXTRA_CFLAGS += -Wextra -Wno-unused-parameter

obj-m		+= ib_srpt.o
