#
# makefile for public domain ndbm-clone: sdbm
# DUFF: use duff's device (loop unroll) in parts of the code
#
CFLAGS = -O -DSDBM -DDUFF -DBSD42 -pic
#LDFLAGS = -p

OBJS = sdbm.o pair.o hash.o
SRCS = sdbm.c pair.c hash.c dbu.c dba.c dbd.c util.c
HDRS = tune.h sdbm.h pair.h
MISC = README CHANGES COMPARE sdbm.3 dbe.c dbe.1 dbm.c dbm.h biblio \
       readme.ms readme.ps

all: dbu dba dbd dbe

dbu: dbu.o sdbm util.o
	cc $(LDFLAGS) -o dbu dbu.o util.o libsdbm.a

dba: dba.o util.o
	cc $(LDFLAGS) -o dba dba.o util.o
dbd: dbd.o util.o
	cc $(LDFLAGS) -o dbd dbd.o util.o
dbe: dbe.o sdbm
	cc $(LDFLAGS) -o dbe dbe.o libsdbm.a

sdbm: $(OBJS)
	ar cr libsdbm.a $(OBJS)
	ranlib libsdbm.a
###	cp libsdbm.a /usr/lib/libsdbm.a

dba.o: sdbm.h
dbu.o: sdbm.h
util.o:sdbm.h

$(OBJS): sdbm.h tune.h pair.h

#
# dbu using berkelezoid ndbm routines [if you have them] for testing
#
#x-dbu: dbu.o util.o
#	cc $(CFLAGS) -o x-dbu dbu.o util.o
lint:
	lint -abchx $(SRCS)

clean:
	rm -f *.o mon.out core

purge: 	clean
	rm -f dbu libsdbm.a dbd dba dbe x-dbu *.dir *.pag

shar:
	shar $(MISC) makefile $(SRCS) $(HDRS) >SDBM.SHAR

readme:
	nroff -ms readme.ms | col -b >README
