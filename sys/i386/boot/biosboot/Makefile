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
# Revision 1.6  1993/07/11  12:02:17  andrew
# Fixes from bde, including support for loading @ any MB boundary (e.g. a
# kernel linked for 0xfe100000 will load at the 1MB mark) and read-ahead
# buffering to speed booting from floppies.  Also works with aha174x
# controllers in enhanced mode.
#
# Revision 1.5  1993/07/04  14:03:03  cgd
# do the right thing, so things aren't built in /usr/src/sys
#
# Revision 1.4  1993/06/18  08:03:14  cgd
# install floppy boot block, too.
#
# Revision 1.3  1993/06/08  03:36:39  deraadt
# makes more sense if src is mounted readonly
#
# Revision 1.2  1993/04/28  08:45:51  deraadt
# Ensure the rmaouthdr shell script is executable before running it!
#
# Revision 1.1  1993/03/21  18:08:11  cgd
# after 0.2.2 "stable" patches applied
#
# 
# 93/06/30  bde
#	fd boot is now fast enough.
#	(Safe) target fd0 and (unsafe) target wd0 to temporarily avoid
#	updating /use/mdec.
#
# 93/06/29  bde
#	Deleted weirdness for not-really-separate code and data segments.
#	It is not necessary now that the GDT is dynamically initialized.
#	table.o doesn't need to be early any more.  boot.sed is not used
#	any more (delete it).  The shell commands that failed with status
#	0 no longer exist so the 93/06/28 fixes got deleted.
#
#	Use .S extension for asm files to get (g)cc to preprocess them
#	automatically.
#
# 93/06/28  bde
#	Don't exit 0 from failing shell commands.
#
# Thu Sep 10 22:40:23 PDT 1992
# Ported to 386BSD by Julian Elischer
# julian@tfs.com
#
# Revision 2.2	92/04/04  11:33:46  rpd
#	Fabricated for MACH 3.0
#	[92/03/30	     mg32]
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
	$(LD) -N -T 0 -o boot $(OBJS) $(LIBS)
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

clean cleandir:
	/bin/rm -f *.o *.d boot bootbios biosboot boot.sym

.include <bsd.prog.mk>
