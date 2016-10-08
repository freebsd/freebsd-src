LIBHNJ = ../libhnj
BST = ../../stuff/bst
VPATH=..
OBJ = t10.o t6.o hytab.o n1.o n2.o n3.o n4.o n5.o \
	n7.o n8.o n9.o ni.o nii.o suftab.o makedev.o afm.o otf.o unimap.o \
	version.o fontmap.o $(BST)/bst.o

FLAGS = -DUSG $(EUC) -I. -I.. -I../../include -DMACDIR='"$(MACDIR)"' \
	-DFNTDIR='"$(FNTDIR)"' -DTABDIR='"$(TABDIR)"' -DHYPDIR='"$(HYPDIR)"' \
	-DSHELL='"$(SHELL)"' -DRELEASE='"$(RELEASE)"' $(DEFINES) -I$(BST)

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all: troff ta otfdump

troff: $(OBJ) $(LIBHNJ)/libhnj.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -L$(LIBHNJ) -lhnj $(LIBS) -o troff

ta: draw.o ta.o
	$(CC) $(CFLAGS) $(LDFLAGS) draw.o ta.o $(LIBS) -lm -o $@

otfdump: otfdump.o otfdump_vs.o
	$(CC) $(CFLAGS) $(LDFLAGS) otfdump.o otfdump_vs.o $(LIBS) -o $@

install:
	$(INSTALL) -c troff $(ROOT)$(BINDIR)/troff
	$(STRIP) $(ROOT)$(BINDIR)/troff
	$(INSTALL) -c ta $(ROOT)$(BINDIR)/ta
	$(STRIP) $(ROOT)$(BINDIR)/ta
	$(INSTALL) -c otfdump $(ROOT)$(BINDIR)/otf_info
	$(STRIP) $(ROOT)$(BINDIR)/otf_info
	$(INSTALL) -c -m 644 troff.1 $(ROOT)$(MANDIR)/man1/troff.1
	$(INSTALL) -c -m 644 otfdump.1 $(ROOT)$(MANDIR)/man1/otf_info.1

clean:
	rm -f $(OBJ) draw.o ta.o troff ta otfdump otfdump.o otfdump_vs.o \
		core log *~

mrproper: clean

draw.o: draw.c
makedev.o: makedev.c dev.h
t10.o: t10.c ../tdef.h ../ext.h dev.h afm.h unimap.h troff.h
t6.o: t6.c ../tdef.h dev.h ../ext.h afm.h unimap.h troff.h
unimap.o: unimap.h
ta.o: ta.c dev.h
hytab.o: ../hytab.c
malloc.o: ../malloc.c ../mallint.h
n1.o: ../n1.c ../tdef.h ../ext.h ./pt.h
n2.o: ../n2.c ../tdef.h ./pt.h ../ext.h
n3.o: ../n3.c ../tdef.h ./pt.h ../ext.h
n4.o: ../n4.c ../tdef.h ./pt.h ../ext.h
n5.o: ../n5.c ../tdef.h ./pt.h ../ext.h
n7.o: ../n7.c ../tdef.h ./pt.h ../ext.h
n8.o: ../n8.c ../tdef.h ../ext.h ./pt.h
n9.o: ../n9.c ../tdef.h ./pt.h ../ext.h
ni.o: ../ni.c ../tdef.h ./pt.h ../ext.h
nii.o: ../nii.c ../tdef.h ./pt.h ../ext.h
suftab.o: ../suftab.c
version.o: ../version.c
otfdump_vs.o: ../version.c
afm.o: dev.h afm.h
otf.o: dev.h afm.h unimap.h
otfdump.o: afm.h afm.c otf.c otfdump.c dpost.d/getopt.c dev.h
fontmap.o: fontmap.h
