# Makefile for the Linux video drivers.
# 28 Jan 2001, Helge Deller, <mailto:hdeller@redhat.com>
# Rewritten to use lists instead of if-statements.

O_TARGET := sti.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs := sticore.o sticon.o stifb.o

# Each configuration option enables a list of files.

obj-$(CONFIG_FB_STI)		+= sticore.o stifb.o
obj-$(CONFIG_STI_CONSOLE)	+= sticore.o sticon.o

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s
