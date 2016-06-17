#
# Makefile for common code of NEC vr4181 based boards
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= vr4181.o

obj-y	 := irq.o int_handler.o serial.o time.o

include $(TOPDIR)/Rules.make
