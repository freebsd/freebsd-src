all:

install: all
	test -d $(ROOT)$(HYPDIR) || mkdir -p $(ROOT)$(HYPDIR)
	for i in *.dic; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(HYPDIR)/$$i || exit; \
	done

clean:

mrproper: clean
