#
#	Makefile for PCMCIA card programs.
#
CFLAGS = -m486 -g # -DDEBUG

PROGS = dumpcis rdmap rdreg wrreg pccardmem wrattr enabler

all: $(PROGS)

clean:
	rm -f *.core core *.o $(PROGS)

dumpcis: dumpcis.o readcis.o printcis.o
	cc -o dumpcis -static dumpcis.o readcis.o printcis.o

wrattr: wrattr.c
	cc $(CFLAGS) -o wrattr wrattr.c

pccardmem: pccardmem.c
	cc $(CFLAGS) -o pccardmem pccardmem.c

enabler: enabler.c
	cc $(CFLAGS) -o enabler enabler.c

rdmap: rdmap.c
	cc $(CFLAGS) -o rdmap rdmap.c

rdreg: rdreg.c
	cc $(CFLAGS) -o rdreg rdreg.c

wrreg: wrreg.c
	cc $(CFLAGS) -o wrreg wrreg.c
