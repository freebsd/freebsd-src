VPATH=..

FONTS = AB AI AR AX BI CB CI CO CW CX GR HB HI HX Hb Hi Hr Hx \
	KB KI KR KX NB NI NR NX PA PB PI PX S1 VB VI VR VX ZD ZI B H I R S

FLAGS = -I. -I.. -DFNTDIR='"$(FNTDIR)"'

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(CPPFLAGS) $(FLAGS) -c $<

all:

install: all
	test -d $(ROOT)$(FNTDIR) || mkdir -p $(ROOT)$(FNTDIR)
	test -d $(ROOT)$(FNTDIR)/devpost/charlib || \
		mkdir -p $(ROOT)$(FNTDIR)/devpost/charlib
	cd devpost && for i in ? ?? H?? H??? ?.name ??.name H??.name \
	    H???.name DESC* FONTMAP; do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(FNTDIR)/devpost/ || exit; \
	done
	cd $(ROOT)$(FNTDIR)/devpost && \
		for i in G HM HK HL; \
		do \
			rm -f $$i; ln -s H $$i || exit; \
		done && \
		rm -f GI; ln -s HI GI
	cd devpost/charlib && for i in ?? ??.map BRACKETS_NOTE README OLD_LH* \
	    LH_uc; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(FNTDIR)/devpost/charlib \
			|| exit; \
	done
	test -d $(ROOT)$(FNTDIR)/devps || mkdir -p $(ROOT)$(FNTDIR)/devps
	cd devps && for i in ? ?.afm ?? ??.afm DESC MustRead.html FONTMAP; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(FNTDIR)/devps/ || exit; \
	done
	rm -f $(ROOT)$(FNTDIR)/devps/charlib
	ln -s ../devpost/charlib $(ROOT)$(FNTDIR)/devps/charlib
	rm -f $(ROOT)$(FNTDIR)/devps/postscript
	ln -s ../devpost/postscript $(ROOT)$(FNTDIR)/devps/postscript
	for j in devpslow devpsmed; \
	do \
		test -d $(ROOT)$(FNTDIR)/$$j || mkdir -p $(ROOT)$(FNTDIR)/$$j; \
		$(INSTALL) -c -m 644 $$j/DESC $(ROOT)$(FNTDIR)/$$j/; \
		(cd $(ROOT)$(FNTDIR)/devps && for i in *; \
		do \
			test $$i != DESC || continue; \
			rm -f ../$$j/$$i; \
			ln -s ../devps/$$i ../$$j/$$i ; \
		done); \
	done

clean:
	rm -f core log *~

mrproper: clean
