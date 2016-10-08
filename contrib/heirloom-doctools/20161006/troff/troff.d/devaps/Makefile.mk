OBJ = daps.o build.o draw.o getopt.o version.o

FONTS = B I R S CT CW CX GB GI GR GS HI HK HX PO PX S1 SC SM TX DESC \
	C G H BI CE CI HB HL MB MI MR MX PA PB PI TB

FLAGS = -I. -I.. -DFNTDIR='"$(FNTDIR)"'

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all: daps makedev fonts HM.out

daps: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -lm -o daps

makedev: makedev.o
	$(CC) $(LDFLAGS) makedev.o $(LIBS) -o makedev

fonts: makedev
	for i in $(FONTS); \
	do \
		./makedev $$i || exit; \
	done

HM.out: HB.out
	rm -f $@
	ln -s HB.out $@

HB.out: fonts

install: all
	$(INSTALL) -c daps $(ROOT)$(BINDIR)/daps
	$(STRIP) $(ROOT)$(BINDIR)/daps
	mkdir -p $(ROOT)$(FNTDIR)/devaps
	for i in $(FONTS) *.add *.out version; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(FNTDIR)/devaps/$$i || exit; \
	done

clean:
	rm -f $(OBJ) daps makedev.o makedev *.add *.out core log *~

mrproper: clean

build.o: build.c daps.h
daps.o: daps.c aps.h dev.h daps.h daps.g
makedev.o: makedev.c dev.h
draw.o: draw.c ../draw.c
