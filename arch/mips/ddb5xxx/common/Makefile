#
# Makefile for the common code of NEC DDB-Vrc5xxx board
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= ddb5xxx.o

obj-y	 += irq.o nile4.o prom.o rtc_ds1386.o

include $(TOPDIR)/Rules.make
