#
# Makefile for Etrax-specific library files..
#

.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -traditional -c $< -o $*.o

L_TARGET = lib.a
obj-y  = checksum.o checksumcopy.o string.o usercopy.o memset.o csumcpfruser.o

include $(TOPDIR)/Rules.make
