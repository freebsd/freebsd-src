#
# Makefile for the IBM WorkPad z50 specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := workpad.o

all: workpad.o

obj-y	:= init.o setup.o

include $(TOPDIR)/Rules.make
