#	from: @(#)Makefile	7.9 (Berkeley) 5/8/91
#	$Id: Makefile,v 1.2 1993/10/16 18:49:18 rgrimes Exp $
#

DESTDIR=/usr
LD=/usr/bin/ld
STAND=	/sys/stand
INCPATH=-I/sys/sys -I/sys -I/sys/ufs  -I${STAND}
VPATH=	${STAND}
STANDDIR= ${DESTDIR}/mdec

CC=	cc -traditional
CPP=	cpp -traditional ${INCPATH} -DSTANDALONE -DAT386

RELOC=	98000
RELOC2=	98200

CFLAGS=	-DSTANDALONE -DAT386 -O ${INCPATH}

DRIVERS=cga.c fd.c kbd.c wd.c as.c
SRCS=	boot.c fdbootblk.c prf.c \
	srt0.c wdbootblk.c ${DRIVERS} ${SASRC}

ALL= wdboot bootwd fdboot bootfd asboot bootas

all: ${ALL}

# startups

srt0.o: srt0.c
	${CPP} -E -DLOCORE -DRELOC=0x${RELOC} srt0.c | ${AS} -o srt0.o

wsrt0.o: srt0.c
	${CPP} -E -DLOCORE -DSMALL -DRELOC=0x${RELOC} -DREL srt0.c | \
	    ${AS} -o wsrt0.o

relsrt0.o: srt0.c
	${CPP} -E -DLOCORE -DRELOC=0x${RELOC} -DREL srt0.c | ${AS} -o relsrt0.o

# block 0 boots

wdbootblk.o: wdbootblk.c 
	${CPP} -E -DLOCORE -DRELOC=0x${RELOC} wdbootblk.c | ${AS} -o $@

fdbootblk.o: fdbootblk.c 
	${CPP} -E -DLOCORE -DRELOC=0x${RELOC} fdbootblk.c | ${AS} -o $@

asbootblk.o: asbootblk.c 
	cc -c -O -DRELOC=0x${RELOC} ${INCPATH} asbootblk.c

# getting booted from disc

wdboot: wdbootblk.o trimhd
	${LD} -N -T ${RELOC} wdbootblk.o
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

bootwd:	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o wd.o printf.o breadwd.o trimhd
	${LD} -N -T ${RELOC2} wsrt0.o boot.o bmap.o cga.o kbd.o prf.o printf.o \
		breadwd.o fs.o wd.o -lc
	size a.out
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

fdboot: fdbootblk.o trimhd
	${LD} -N -T ${RELOC} fdbootblk.o
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

bootfd:	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o fd.o printf.o breadfd.o trimhd
	${LD} -N -T ${RELOC2} wsrt0.o boot.o bmap.o cga.o kbd.o prf.o printf.o \
		breadfd.o fs.o fd.o -lc
	size a.out
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

asboot:	asbootblk.o trimhd
	${LD} -N -T 7c00 asbootblk.o
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

bootas:	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o as.o printf.o breadas.o trimhd
	${LD} -N -T ${RELOC2} wsrt0.o boot.o bmap.o cga.o kbd.o prf.o printf.o \
		breadas.o fs.o as.o -lc
	size a.out
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@


breadwd.o: breadwd.c breadxx.o
breadfd.o: breadfd.c breadxx.o
breadas.o: breadas.c breadxx.o

breadxx.o:
	touch breadxx.o

breadwd.c: breadxx.c
	rm -f breadwd.c
	sed -e 's/XX/wd/' -e 's/xx/wd/g'	< breadxx.c >> breadwd.c

breadfd.c: breadxx.c
	rm -f breadfd.c
	sed -e 's/XX/fd/' -e 's/xx/fd/g'	< breadxx.c >> breadfd.c

breadas.c: breadxx.c
	rm -f breadas.c
	sed -e 's/XX/as/' -e 's/xx/as/g'	< breadxx.c >> breadas.c

trimhd:	trimhd.c
	${CC} ${CFLAGS} -s -o trimhd trimhd.c

clean:
	rm -f *.o *.exe *.i sm_*.c trimhd
	rm -f a.out bfd bwd fdb wdb ${ALL}
	rm -f boot[a-wyz]? boot[a-wyz]?? boot[a-wyz]?.c boot[a-wyz]??.c \
		conf[a-wyz]?.c conf[a-wyz]??.c bread[a-wyz]?.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep ${INCPATH} -DSTANDALONE ${SRCS} ${DUMMIES}

install: ${ALL}
	cp ${ALL} ${STANDDIR}
