#
# Ported to boot 386BSD by Julian Elischer (julian@tfs.com)
# September 1992
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# Carnegie Mellon requests users of this software to return to
#
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
#
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 2.2	92/04/04  11:33:46  rpd
#	Fabricated for MACH 3.0
#	[92/03/30	     mg32]
#
# Thu Sep 10 22:40:23 PDT 1992
# Ported to 386BSD by Julian Elischer
# julian@tfs.com
#

CFLAGS	= -O -DDO_BAD144
LIBS= -lc
INC= -I../..

# start.o should be first, table.o should be second
OBJS = start.o table.o boot2.o boot.o asm.o bios.o io.o disk.o sys.o

.SUFFIXES: .s .c .o

# These are wierd because we don't want separate code and data segments.. ok?
.c.o:
	@echo $(CC) -c $(CFLAGS) $(INC) $*.c
	-@trap "/bin/rm -f $*.i $*.s; exit 0" 0 1 2 3 10 15; \
	$(CC)  $(CFLAGS) $(INC) -S $<;	\
	if [ $$? != 0 ]; then :; else \
		sed -f boot.sed $*.s > $*.i;	\
		$(AS) $*.i -o $@; \
	fi

.s.o:
	@echo $(AS) -o $*.o $<
	-@trap "/bin/rm -f $*.i X$*.c; exit 0" 0 1 2 3 10 15; \
	/bin/rm -f X$*.c;	\
	ln $*.s X$*.c;	\
	$(CC) -E $(CFLAGS) X$*.c > $*.i;	\
	if [ $$? != 0 ]; then :;	\
	else \
		$(AS) $*.i -o $@;	\
	fi


boot: boot.sed $(OBJS)
	$(LD) -N -T 0 -o boot $(OBJS) $(LIBS)
	cp boot boot.sym
	@strip boot
	@./rmaouthdr boot boot.tmp
	@mv -f boot.tmp boot
	@ls -l boot

biosboot:	boot
	dd if=boot of=biosboot count=1

bootbios:	boot
	dd if=boot of=bootbios skip=1

/usr/mdec/bootsd:	bootbios
	cp bootbios /usr/mdec/bootsd

/usr/mdec/sdboot:	biosboot
	cp biosboot /usr/mdec/sdboot

/usr/mdec/bootwd:	/usr/mdec/bootsd
	rm -f /usr/mdec/bootwd
	ln /usr/mdec/bootsd /usr/mdec/bootwd

/usr/mdec/wdboot:	/usr/mdec/sdboot
	rm -f /usr/mdec/wdboot
	ln /usr/mdec/sdboot /usr/mdec/wdboot

/usr/mdec/bootfd:	/usr/mdec/bootsd
	rm -f /usr/mdec/bootfd
	ln /usr/mdec/bootsd /usr/mdec/bootfd

/usr/mdec/fdboot:	/usr/mdec/sdboot
	rm -f /usr/mdec/fdboot
	ln /usr/mdec/sdboot /usr/mdec/fdboot

sd:	/usr/mdec/bootsd /usr/mdec/sdboot
wd:	/usr/mdec/bootwd /usr/mdec/wdboot
fd:	/usr/mdec/bootfd /usr/mdec/fdboot

#wd0:	/usr/mdec/bootwd /usr/mdec/wdboot
#	disklabel -r -w wd0 julian julian5 /usr/mdec/wdboot /usr/mdec/bootwd
#

sd0:	/usr/mdec/bootsd /usr/mdec/sdboot
	disklabel -r -w sd0 XT-8760 scsitest /usr/mdec/sdboot /usr/mdec/bootsd


#fd0:	/usr/mdec/bootfd /usr/mdec/fdboot
#	disklabel -r -w fd0 floppy5 bootflpy /usr/mdec/fdboot /usr/mdec/bootfd
#

install: wd sd 
# you should use the old floppy booter, it's MUCH faster
# This one works but it's too slow.
# besides how many kernels can you fit on a floppy?
# only use this one if you want to boot a kernel
# from a hard disk 2 when you've trashed hard disk 1
#install: wd sd fd

clean:
	/bin/rm -f *.o *.d boot bootbios biosboot boot.sym


