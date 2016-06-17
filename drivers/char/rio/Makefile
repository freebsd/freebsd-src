#
# Makefile for the linux rio-subsystem.
#
# (C) R.E.Wolff@BitWizard.nl 
# 
# This file is GPL. See other files for the full Blurb. I'm lazy today. 
#
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := rio.o

obj-y   := rio_linux.o rioinit.o rioboot.o riocmd.o rioctrl.o riointr.o \
            rioparam.o riopcicopy.o rioroute.o riotable.o riotty.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
