all: pcic.o ed.o

CFLAGS = -O -DINET -DKERNEL -DLKM -I/sys

pcic.o: pcic.c
	cc -c $(CFLAGS) pcic.c

load: pcic.o
	modload -e lkm_pcic pcic.o
	pccardmem d0000

unload:
	modunload -n pcic

skel.o: skel.c
	cc -c $(CFLAGS) skel.c

ldskel: skel.o
	modload -e lkm_skel skel.o

ed.o: if_ed.o lkm_ed.o
	ld -r -o ed.o if_ed.o lkm_ed.o

lded: ed.o
	modload -e lkm_ed ed.o
