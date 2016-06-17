# $Id: Makefile,v 1.10 2000/02/23 08:17:46 jj Exp $
# Makefile for the Sparc boot stuff.
#
# Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
# Copyright (C) 1997,1998 Jakub Jelinek (jj@ultra.linux.cz)

ROOT_IMG	=/usr/src/root.img
ELFTOAOUT	=elftoaout

all: btfix.o

tftpboot.img: piggyback
	$(ELFTOAOUT) $(TOPDIR)/vmlinux -o tftpboot.img
	./piggyback tftpboot.img $(TOPDIR)/System.map $(ROOT_IMG)

piggyback: piggyback.c
	$(HOSTCC) $(HOSTCFLAGS) -o piggyback piggyback.c

btfixupprep: btfixupprep.c
	$(HOSTCC) $(HOSTCFLAGS) -o btfixupprep btfixupprep.c

clean:
	rm -f btfixupprep piggyback tftpboot.img btfix.o btfix.s

BTOBJS := $(HEAD) init/main.o init/version.o init/do_mounts.o
BTLIBS := $(CORE_FILES_NO_BTFIX) $(FILESYSTEMS) \
	$(DRIVERS) $(NETWORKS)

# I wanted to make this depend upon BTOBJS so that a parallel
# build would work, but this fails because $(HEAD) cannot work
# properly as it will cause head.o to be built with the implicit
# rules not the ones in kernel/Makefile.  Someone please fix. --DaveM
vmlinux.o: dummy
	$(LD) -r $(patsubst %,$(TOPDIR)/%,$(BTOBJS)) \
		--start-group \
		$(patsubst %,$(TOPDIR)/%,$(BTLIBS)) \
		$(LIBS) \
		--end-group -o vmlinux.o

btfix.s: btfixupprep vmlinux.o
	$(OBJDUMP) -x vmlinux.o | ./btfixupprep > btfix.s

btfix.o: btfix.s
	$(CC) -c -o btfix.o btfix.s

include $(TOPDIR)/Rules.make
