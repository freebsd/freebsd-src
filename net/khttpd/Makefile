#
# Makefile for kHTTPd
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := khttpd.o

obj-m := 	$(O_TARGET)
obj-y := 	main.o accept.o datasending.o logging.o misc.o rfc.o rfc_time.o security.o \
		sockets.o sysctl.o userspace.o waitheaders.o


include $(TOPDIR)/Rules.make

rfc_time.o: times.h

make_times_h: make_times_h.c
	$(HOSTCC) $(HOSTCFLAGS) -o make_times_h make_times_h.c

times.h: make_times_h
	./make_times_h
