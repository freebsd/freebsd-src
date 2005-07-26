# $FreeBSD$

OBJS=	bpfstat.o
CC?=	CC
PROG=	bpfstat
PREFIX?=/usr/local

bpfstat:	$(OBJS)
		$(CC) -lm -o $(PROG) $(OBJS)
install:
	cp $(PROG) $(PREFIX)/sbin
	cp $(PROG).8 $(PREFIX)/man/man8/

clean:
	rm -f *.o $(PROG)

all:	bpfstat
