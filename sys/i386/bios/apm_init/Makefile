#
# APM (Advanced Power Management) BIOS Device Driver
#
# Copyright (c) 1994-1995 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
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
#	$Id: Makefile,v 1.7 1997/03/09 16:46:04 kato Exp $
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

apm_init.inc: apm_initat.inc apm_init98.inc
	echo "#ifdef PC98" > apm_init.inc
	cat apm_init98.inc >> apm_init.inc
	echo "#else" >> apm_init.inc
	cat apm_initat.inc >> apm_init.inc
	echo "#endif" >> apm_init.inc

apm_initat.inc: apm_initat bin2asm
	./bin2asm apm_initat apm_initat.inc

apm_initat: $(OBJS)
	rm -f $(OBJS)
	make $(OBJS)
	$(LD) -Bstatic -N -T 0 -o apm_initat $(OBJS)
	cp apm_initat apm_initat.sym
	@strip apm_initat
	@sh ${.CURDIR}/rmaouthdr apm_initat apm_initat.tmp
	@mv -f apm_initat.tmp apm_initat

apm_init98.inc: apm_init98 bin2asm
	./bin2asm apm_init98 apm_init98.inc

apm_init98: $(OBJS)
	rm -f $(OBJS)
	make CC="$(CC) -DPC98" $(OBJS)
	$(LD) -Bstatic -N -T 0 -o apm_init98 $(OBJS)
	cp apm_init98 apm_init98.sym
	@strip apm_init98
	@sh ${.CURDIR}/rmaouthdr apm_init98 apm_init98.tmp
	@mv -f apm_init98.tmp apm_init98

bin2asm: bin2asm.c
	$(CC) -o bin2asm ${.CURDIR}/bin2asm.c

allclean: clean
	@rm -f apm_init.inc
clean:
	@rm -f *.o apm_init98 apm_init98.inc apm_init98.sym \
		apm_initat apm_initat.inc apm_initat.sym apm_init apm_init.sym bin2asm

.include <bsd.prog.mk>
