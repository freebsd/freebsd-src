#
# Makefile for s390-specific library files..
#

.S.o:
	$(CC) $(AFLAGS) -traditional -c $< -o $*.o

L_TARGET = lib.a

obj-y = checksum.o delay.o memset.o misaligned.o strcmp.o strncpy.o uaccess.o
export-objs += misaligned.o

include $(TOPDIR)/Rules.make

