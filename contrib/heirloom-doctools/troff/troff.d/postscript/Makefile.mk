all:

install:
	test -d $(ROOT)$(PSTDIR) || mkdir -p $(ROOT)$(PSTDIR)
	for i in *.ps ps.requests; \
	do \
		$(INSTALL) -c -m 644 $$i $(ROOT)$(PSTDIR)/$$i || exit; \
	done

clean:

mrproper: clean
