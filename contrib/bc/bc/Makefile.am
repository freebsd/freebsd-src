## Process this file with automake to produce Makefile.in
bin_PROGRAMS = bc

bc_SOURCES = main.c bc.y scan.l execute.c load.c storage.c util.c global.c

EXTRA_DIST = bc.h fix-libmath_h libmath.b sbc.y
noinst_HEADERS = libmath.h

DISTCLEANFILES = sbc sbc.c sbc.h
MAINTAINERCLEANFILES = libmath.h bc.c bc.h 

datadir = $(prefix)/@DATADIRNAME@

INCLUDES = -I$(srcdir) -I$(srcdir)/../h
LDADD = ../lib/libbc.a @LEXLIB@

$(PROGRAMS): $(LDADD)

scan.o: bc.h
global.o: libmath.h

libmath.h: libmath.b
	echo \"\" > libmath.h
	$(MAKE) fbc
	./fbc -c $(srcdir)/libmath.b </dev/null >libmath.h
	$(srcdir)/fix-libmath_h
	rm -f ./fbc

install-data-local:
	if grep -s "define BC_MATH_FILE" ../config.h; \
	  then $(mkinstalldirs) $(libdir); \
	  rm -f $(libdir)/libmath.b; \
	  $(INSTALL_DATA) $(srcdir)/libmath.b $(libdir); \
	  chmod 444 $(libdir)/libmath.b; \
	  else true; \
	fi

fbcOBJ = main.o bc.o scan.o execute.o global.o load.o storage.o util.o
fbc: $(fbcOBJ)
	$(LINK) $(fbcOBJ) $(LDADD) $(LIBS)


sbcOBJ = main.o sbc.o scan.o execute.o global.o load.o storage.o util.o
sbc.o: sbc.c
sbc: $(sbcOBJ)
	$(LINK) $(sbcOBJ) $(LDADD) $(LIBS)
