BST = ../../../stuff/bst

OBJ = dpost.o draw.o color.o pictures.o ps_include.o afm.o \
	makedev.o glob.o misc.o request.o version.o getopt.o \
	asciitype.o otf.o ../fontmap.o $(BST)/bst.o

FLAGS = -I. -I.. -DFNTDIR='"$(FNTDIR)"' -DPSTDIR='"$(PSTDIR)"' $(EUC) \
	$(DEFINES) -I../../../include -I.. -I$(BST)

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all: dpost

dpost: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o dpost

install:
	$(INSTALL) -c dpost $(ROOT)$(BINDIR)/dpost
	$(STRIP) $(ROOT)$(BINDIR)/dpost
	mkdir -p $(ROOT)$(MANDIR)/man1
	$(INSTALL) -c -m 644 dpost.1 $(ROOT)$(MANDIR)/man1/dpost.1

clean:
	rm -f $(OBJ) dpost core log *~

mrproper: clean

color.o: color.c gen.h ext.h
dpost.o: dpost.c comments.h gen.h path.h ext.h ../dev.h dpost.h ../afm.h \
	asciitype.h
draw.o: draw.c gen.h ext.h
glob.o: glob.c gen.h
misc.o: misc.c gen.h ext.h path.h asciitype.h
pictures.o: pictures.c comments.h gen.h path.h
ps_include.o: ps_include.c ext.h gen.h asciitype.h path.h
request.o: request.c gen.h request.h path.h
afm.o: afm.c ../dev.h ../afm.h ../afm.c
otf.o: otf.c ../dev.h ../afm.h ../otf.c
makedev.o: makedev.c ../dev.h ../makedev.c
asciitype.o: asciitype.h
version.o: version.c ../../version.c
