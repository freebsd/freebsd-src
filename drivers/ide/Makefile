#
# Makefile for the kernel ata, atapi, and ide block device drivers.
#
# 12 September 2000, Bartlomiej Zolnierkiewicz <bkz@linux-ide.org>
# Rewritten to use lists instead of if-statements.
#
# Note : at this point, these files are compiled on all systems.
# In the future, some of these should be built conditionally.
#


export-objs := ide-iops.o ide-taskfile.o ide-proc.o ide.o ide-probe.o ide-probe-mini.o ide-dma.o ide-lib.o setup-pci.o ide-io.o ide-disk.o

all-subdirs	:= arm legacy pci ppc raid
mod-subdirs	:= arm legacy pci ppc raid

obj-y		:=
obj-m		:=
ide-obj-y	:=

subdir-$(CONFIG_BLK_DEV_IDE) += legacy ppc arm raid pci

# First come modules that register themselves with the core

ifeq ($(CONFIG_BLK_DEV_IDE),y)
  obj-y		+= pci/idedriver-pci.o
endif

# Core IDE code - must come before legacy

ide-core-objs	:= ide-iops.o ide-taskfile.o ide.o ide-lib.o ide-io.o \
		   ide-default.o ide-probe.o ide-geometry.o
ide-detect-objs	:= ide-probe-mini.o

ifeq ($(CONFIG_PROC_FS),y)
ide-core-objs	+= ide-proc.o
endif
ifeq ($(CONFIG_BLK_DEV_IDEPCI),y)
ide-core-objs += setup-pci.o
endif
ifeq ($(CONFIG_BLK_DEV_IDEDMA_PCI),y)
ide-core-objs += ide-dma.o
endif

# Initialisation order:
#	Core sets up
#	Legacy drivers may register a callback
#	Drivers are pre initialised
#	Probe inits the drivers and driver callbacks
#	Raid scans the devices

obj-$(CONFIG_BLK_DEV_IDE)		+= ide-core.o

ifeq ($(CONFIG_BLK_DEV_IDE),y)
  obj-y		+= legacy/idedriver-legacy.o
  obj-y		+= ppc/idedriver-ppc.o
  obj-y		+= arm/idedriver-arm.o
else
  ifeq ($(CONFIG_BLK_DEV_HD_ONLY),y)
	subdir-$(CONFIG_BLK_DEV_HD_ONLY) += legacy
	obj-y	+= legacy/idedriver-legacy.o
  endif
endif

obj-$(CONFIG_BLK_DEV_ISAPNP) 		+= ide-pnp.o

obj-$(CONFIG_BLK_DEV_IDEDISK)		+= ide-disk.o
obj-$(CONFIG_BLK_DEV_IDECD)		+= ide-cd.o
obj-$(CONFIG_BLK_DEV_IDETAPE)		+= ide-tape.o
obj-$(CONFIG_BLK_DEV_IDEFLOPPY)		+= ide-floppy.o

obj-$(CONFIG_BLK_DEV_IDE) += ide-detect.o

ifeq ($(CONFIG_BLK_DEV_IDE),y)
# RAID must be last of all
  obj-y		+= raid/idedriver-raid.o
endif

list-multi	:= ide-core.o ide-detect.o
O_TARGET := idedriver.o

include $(TOPDIR)/Rules.make

ide-core.o:	$(ide-core-objs)
	$(LD) -r -o $@ $(ide-core-objs)

ide-detect.o:	$(ide-detect-objs)
	$(LD) -r -o $@ $(ide-detect-objs)

