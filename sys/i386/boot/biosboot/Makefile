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
#	from: Mach, Revision 2.2	92/04/04  11:33:46  rpd
#	$Id: Makefile,v 1.4 1993/11/16 02:02:05 ache Exp $
#

wd0:
	dd if=boot of=biosboot count=1
	dd if=boot of=bootbios skip=1
	disklabel -r -w wd0 nec5655 newboot biosboot bootbios
	rm biosboot bootbios

NOPROG=	noprog
NOMAN=	noman

CFLAGS	= -O -DDO_BAD144 -I${.CURDIR}
LIBS= -lc
INC= -I${.CURDIR}/../..

# start.o should be first
OBJS = start.o table.o boot2.o boot.o asm.o bios.o io.o disk.o sys.o

.SUFFIXES: .S .c .o

.c.o:
	$(CC)  $(CFLAGS) $(INC) -c $<

.S.o:
	$(CC)  $(CFLAGS) -c $<

boot: $(OBJS)
	$(LD) -Bstatic -N -T 0 -o boot $(OBJS) $(LIBS)
	cp boot boot.sym
	@strip boot
	@sh ${.CURDIR}/rmaouthdr boot boot.tmp
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

all: biosboot bootbios

fd0:
	dd if=boot of=biosboot count=1
	dd if=boot of=bootbios skip=1
	disklabel -r -w fd0 floppy bootflpy biosboot bootbios
	rm biosboot bootbios

install: wd sd fd

clean:
	/bin/rm -f *.o *.d boot bootbios biosboot boot.sym

.include <bsd.prog.mk>
