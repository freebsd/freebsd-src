ROBJ = glue1.o refer1.o refer2.o refer4.o refer5.o refer6.o mkey3.o \
	refer7.o refer8.o hunt2.o hunt3.o deliv2.o hunt5.o hunt6.o \
	hunt8.o glue3.o hunt7.o hunt9.o glue2.o glue4.o glue5.o refer0.o \
	shell.o version.o
AOBJ = addbib.o version.o
LOBJ = lookbib.o version.o
SOBJ = sortbib.o version.o
MOBJ = mkey1.o mkey2.o mkey3.o deliv2.o version.o
IOBJ = inv1.o inv2.o inv3.o inv5.o inv6.o deliv2.o version.o
HOBJ = hunt1.o hunt2.o hunt3.o hunt5.o hunt6.o hunt7.o glue5.o refer3.o \
	hunt9.o shell.o deliv2.o hunt8.o glue4.o tick.o version.o


FLAGS =	-DMACDIR='"$(MACDIR)"' -DREFDIR='"$(REFDIR)"' $(EUC) $(DEFINES) \
	-I../include

.c.o:
	$(CC) $(CFLAGS) $(WARN) $(FLAGS) $(CPPFLAGS) -c $<

all: refer addbib lookbib sortbib roffbib indxbib mkey inv hunt papers/runinv

refer: $(ROBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(ROBJ) $(LIBS) -o $@

addbib: $(AOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(AOBJ) $(LIBS) -o $@

lookbib: $(LOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LOBJ) $(LIBS) -o $@

sortbib: $(SOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOBJ) $(LIBS) -o $@

mkey: $(MOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(MOBJ) $(LIBS) -o $@

inv: $(IOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(IOBJ) $(LIBS) -o $@

hunt: $(HOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $(HOBJ) $(LIBS) -o $@

indxbib: indxbib.sh
	rm -f $@
	echo '#!$(SHELL)' >$@
	sed 's:@REFDIR@:$(REFDIR):g' indxbib.sh >>$@

roffbib: roffbib.sh
	rm -f $@
	echo '#!$(SHELL)' >$@
	sed 's:@BINDIR@:$(BINDIR):g' roffbib.sh >>$@

papers/runinv: papers/runinv.sh
	rm -f $@
	echo '#!$(SHELL)' >$@
	sed 's:@REFDIR@:$(REFDIR):g' papers/runinv.sh >>$@

install: all
	for i in refer addbib lookbib sortbib; \
	do \
		$(INSTALL) -c $$i $(ROOT)$(BINDIR)/$$i || exit; \
		$(STRIP) $(ROOT)$(BINDIR)/$$i || exit; \
	done
	$(INSTALL) -c roffbib $(ROOT)$(BINDIR)/roffbib
	$(INSTALL) -c indxbib $(ROOT)$(BINDIR)/indxbib
	test -d $(ROOT)$(REFDIR) || mkdir -p $(ROOT)$(REFDIR)
	for i in hunt inv mkey; \
	do \
		$(INSTALL) -c $$i $(ROOT)$(REFDIR)/$$i || exit; \
		$(STRIP) $(ROOT)$(REFDIR)/$$i || exit; \
	done
	test -d $(ROOT)$(REFDIR)/papers || mkdir -p $(ROOT)$(REFDIR)/papers
	$(INSTALL) -c -m 644 \
	    papers/Rbstjissue $(ROOT)$(REFDIR)/papers/Rbstjissue
	$(INSTALL) -c -m 644 papers/Rv7man $(ROOT)$(REFDIR)/papers/Rv7man
	$(INSTALL) -c papers/runinv $(ROOT)$(REFDIR)/papers/runinv
	cd $(ROOT)$(REFDIR)/papers && PATH=$(ROOT)$(REFDIR):$$PATH ./runinv
	for i in addbib.1 lookbib.1 refer.1 roffbib.1 sortbib.1; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(MANDIR)/man1/$$i || exit; \
	done
	rm -f $(ROOT)$(MANDIR)/man1/indxbib.1
	ln -s lookbib.1 $(ROOT)$(MANDIR)/man1/indxbib.1

clean:
	rm -f $(ROBJ) refer $(AOBJ) addbib $(LOBJ) lookbib \
		$(SOBJ) sortbib roffbib indxbib $(MOBJ) mkey \
		$(IOBJ) inv $(HOBJ) hunt papers/runinv core log *~

mrproper: clean

addbib.o: addbib.c
deliv2.o: deliv2.c refer..c
glue1.o: glue1.c refer..c
glue2.o: glue2.c refer..c
glue3.o: glue3.c refer..c
glue4.o: glue4.c refer..c
glue5.o: glue5.c refer..c
hunt1.o: hunt1.c refer..c
hunt2.o: hunt2.c refer..c
hunt3.o: hunt3.c refer..c
hunt5.o: hunt5.c
hunt6.o: hunt6.c refer..c
hunt7.o: hunt7.c refer..c
hunt8.o: hunt8.c refer..c
hunt9.o: hunt9.c
inv1.o: inv1.c refer..c
inv2.o: inv2.c refer..c
inv3.o: inv3.c
inv5.o: inv5.c refer..c
inv6.o: inv6.c refer..c
lookbib.o: lookbib.c
mkey1.o: mkey1.c refer..c
mkey2.o: mkey2.c refer..c
mkey3.o: mkey3.c refer..c
refer0.o: refer0.c refer..c
refer1.o: refer1.c refer..c
refer2.o: refer2.c refer..c
refer3.o: refer3.c refer..c
refer4.o: refer4.c refer..c
refer5.o: refer5.c refer..c
refer6.o: refer6.c refer..c
refer7.o: refer7.c refer..c
refer8.o: refer8.c refer..c
shell.o: shell.c
sortbib.o: sortbib.c
tick.o: tick.c
version.o: version.c
