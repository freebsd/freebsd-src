# Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version
# 2 of the License, or (at your option) any later version.

# File: drivers/net/WAN/atiXX50/Makefile
#
# Makefile for the Aurora ESSC based cards
# Specifically the 2520, 4020, 4520, 8520
#

O_TARGET := ASLX.o

obj-y := 8253xini.o 8253xnet.o 8253xsyn.o crc32.o 8253xdbg.o 8253xplx.o 8253xtty.o 8253xchr.o 8253xint.o amcc5920.o 8253xmcs.o 8253xutl.o
obj-m := ASLX.o

#EXTRA_CFLAGS += -DFREEININTERRUPT 

include $(TOPDIR)/Rules.make
