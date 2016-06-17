#
# Makefile for m68k-specific library files..
#

.S.o:
	$(CC) $(AFLAGS) -traditional -c $< -o $@

L_TARGET = lib.a

obj-y		:= ashldi3.o ashrdi3.o lshrdi3.o muldi3.o \
			checksum.o memcmp.o memcpy.o memset.o semaphore.o

include $(TOPDIR)/Rules.make
