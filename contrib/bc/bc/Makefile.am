## Process this file with automake to produce Makefile.in
bin_PROGRAMS = bc

bc_SOURCES = main.c bc.y scan.l execute.c load.c storage.c util.c global.c

EXTRA_DIST = bc.h bcdefs.h const.h fix-libmath_h global.h libmath.b proto.h \
             sbc.y
noinst_HEADERS = libmath.h

DISTCLEANFILES = sbc sbc.c sbc.h

MAINTAINERCLEANFILES = Makefile.in libmath.h bc.c bc.h scan.c

INCLUDES = -I$(srcdir) -I$(srcdir)/../h
LIBBC = ../lib/libbc.a
LIBL = @LEXLIB@
LDADD = $(LIBBC) $(LIBL) @READLINELIB@

YFLAGS = -d

CFLAGS = @CFLAGS@ -Wall -funsigned-char

$(PROGRAMS): $(LIBBC)

scan.o: bc.h
global.o: libmath.h

libmath.h: libmath.b
	echo '{0}' > libmath.h
	$(MAKE) fbc
	./fbc -c $(srcdir)/libmath.b </dev/null >libmath.h
	$(srcdir)/fix-libmath_h
	rm -f ./fbc

fbcOBJ = main.o bc.o scan.o execute.o global.o load.o storage.o util.o
fbc: $(fbcOBJ)
	$(LINK) $(fbcOBJ) $(LIBBC) $(LIBL) $(READLINELIB) $(LIBS)

sbcOBJ = main.o sbc.o scan.o execute.o global.o load.o storage.o util.o
sbc.o: sbc.c
sbc: $(sbcOBJ)
	$(LINK) $(sbcOBJ) $(LIBBC) $(LIBL) $(READLINELIB) $(LIBS)
