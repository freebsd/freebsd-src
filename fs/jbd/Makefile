#
# fs/jbd/Makefile
# 
# Makefile for the linux journaling routines.
#

export-objs := journal.o
O_TARGET := jbd.o

obj-y   := transaction.o commit.o recovery.o checkpoint.o revoke.o journal.o

obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make

