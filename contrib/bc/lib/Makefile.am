## Process this file with automake to produce Makefile.in
noinst_LIBRARIES = libbc.a

INCLUDES = -I. -I.. -I$(srcdir)/../h

libbc_a_SOURCES = getopt.c getopt1.c vfprintf.c number.c

DEFS = @DEFS@ $(DEFSADD)

CFLAGS = @CFLAGS@ -Wall -funsigned-char

MAINTAINERCLEANFILES = Makefile.in number.c

newnumber.o: number.c muldigits.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -DMULDIGITS -o newnumber.o $(srcdir)/number.c

muldigits.h: testmul
	@echo "The following may take up to 10 minutes."
	testmul > muldigits.h

testmul: testmul.o number.o
	$(CC) $(CFLAGS) -o testmul testmul.o number.o

specialnumber: newnumber.o
	cp newnumber.o number.o

