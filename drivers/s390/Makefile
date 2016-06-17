#
# Makefile for the S/390 specific device drivers
#

O_TARGET := io.o

subdir-y := block char misc net
subdir-m := $(subdir-y)

obj-y := s390io.o s390mach.o s390dyn.o ccwcache.o sysinfo.o
export-objs += ccwcache.o s390dyn.o s390io.o

obj-y += $(foreach dir,$(subdir-y),$(dir)/s390-$(dir).o)

include $(TOPDIR)/Rules.make
