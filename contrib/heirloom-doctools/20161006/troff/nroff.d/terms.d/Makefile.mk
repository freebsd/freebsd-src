TABS = tab.2631 tab.2631-c tab.2631-e tab.lp \
	tab.300 tab.300-12 tab.300s tab.300s-12 tab.382 \
	tab.4000a tab.450 tab.450-12 tab.832 \
	tab.37 tab.8510 tab.X tab.tn300

LINKS = tab.300S tab.300S-12 tab.4000A

all: $(TABS) $(LINKS)

tab.2631: a.2631 b.lp
	cat a.2631 b.lp >$@

tab.2631-c: a.2631-c b.lp
	cat a.2631-c b.lp >$@

tab.2631-e: a.2631-e b.lp
	cat a.2631-e b.lp >$@

tab.lp: a.lp b.lp
	cat a.lp b.lp >$@

tab.300: a.300 b.300
	cat a.300 b.300 >$@

tab.300-12: a.300-12 b.300
	cat a.300-12 b.300 >$@

tab.300s: a.300s b.300
	cat a.300s b.300 >$@

tab.300s-12: a.300s-12 b.300
	cat a.300s-12 b.300 >$@

tab.382: a.382 b.300
	cat a.382 b.300 >$@

tab.4000a: a.4000a b.300
	cat a.4000a b.300 >$@

tab.450: a.450 b.300
	cat a.450 b.300 >$@

tab.450-12: a.450-12 b.300
	cat a.450-12 b.300 >$@

tab.832: a.832 b.300
	cat a.832 b.300 >$@

tab.37: ab.37
	cat ab.37 >$@

tab.8510: ab.8510
	cat ab.8510 >$@

tab.X: ab.X
	cat ab.X >$@

tab.tn300: ab.tn300
	cat ab.tn300 >$@

tab.300S: tab.300s
	rm -f $@
	ln -s tab.300s $@

tab.300S-12: tab.300s-12
	rm -f $@
	ln -s tab.300s-12 $@

tab.4000A: tab.4000a
	rm -f $@
	ln -s tab.4000a $@

install: all
	test -d $(ROOT)$(TABDIR) || mkdir -p $(ROOT)$(TABDIR)
	cd $(ROOT)$(TABDIR) && rm -f tab.300S tab.300S-12 tab.4000A
	for i in $(TABS) tab.utf8; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(TABDIR)/$$i || exit; \
	done
	cd $(ROOT)$(TABDIR) || exit 1; \
	test -e tab.300S    || ln -s tab.300s tab.300S       || exit 1; \
	test -e tab.300S-12 || ln -s tab.300s-12 tab.300S-12 || exit 1; \
	test -e tab.4000A   || ln -s tab.4000a tab.4000A     || exit 1;

clean:
	rm -f $(TABS) $(LINKS)

mrproper: clean
