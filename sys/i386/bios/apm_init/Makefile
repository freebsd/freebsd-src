#
# APM (Advanced Power Management) BIOS Device Driver
#
# Copyright (c) 1994-1995 by HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
#
# This software may be used, modified, copied, and distributed, in
# both source and binary form provided that the above copyright and
# these terms are retained. Under no circumstances is the author
# responsible for the proper functioning of this software, nor does
# the author assume any responsibility for damages incurred with its
# use.
#
# Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
#
#	$FreeBSD$
#

OBJS = apm_init.o real_prot.o table.o
CFLAGS = -DKERNEL -DINITIALIZER
OPTFLAGS = -O2
I386INCDIR = ${.CURDIR}/../../include
INC= -I$(I386INCDIR)

all: apm_init.inc

.SUFFIXES: .c .S .o

.c.o:
	$(CC)  $(CFLAGS) $(OPTFLAGS) $(INC) -c $<

.S.o:
	$(CC)  $(CFLAGS) $(INC) -c $<

apm_init.inc: apm_init bin2asm
	./bin2asm apm_init apm_init.inc

apm_init: $(OBJS)
	$(LD) -Bstatic -N -T 0 -o apm_init $(OBJS)
	cp apm_init apm_init.sym
	@strip apm_init
	@sh ${.CURDIR}/rmaouthdr apm_init apm_init.tmp
	@mv -f apm_init.tmp apm_init

bin2asm: bin2asm.c
	$(CC) -o bin2asm ${.CURDIR}/bin2asm.c

allclean: clean
	@rm -f apm_init.inc
clean:
	@rm -f *.o apm_init apm_init.sym bin2asm

.include <bsd.prog.mk>
