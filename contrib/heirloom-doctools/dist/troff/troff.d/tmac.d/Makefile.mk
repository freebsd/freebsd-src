MACS=	acm.me bib chars.me deltext.me e eqn.me \
	float.me footnote.me index.me local.me m mmn mmt ms.acc \
	ms.cov ms.eqn ms.ref ms.tbl ms.ths ms.toc null.me refer.me \
	s sh.me tbl.me thesis.me v vgrind \
	an andoc doc doc-common doc-ditroff doc-nroff doc-syms \
	pictures color pm srefs ptx safe g padj taa naa \
	tmac.gchar an-ext

MAN=	mcolor.7 mpictures.7 man.7 mdoc.7

.SUFFIXES: .in

.in:
	sed 's:@MACDIR@:$(MACDIR):; s:@LIBDIR@:$(LIBDIR):' $< >$@

all: $(MACS) $(MAN)

install: all $(ROOT)$(MACDIR) $(ROOT)$(MANDIR)/man7
	for i in $(MACS); do \
		$(INSTALL) -m 644 $$i $(ROOT)$(MACDIR)/ || exit; \
	done
	for i in $(MAN); do \
		$(INSTALL) -m 644 $$i $(ROOT)$(MANDIR)/man7/ || exit; \
	done

clean:
	rm -f andoc bib doc e g m s pm an doc-ditroff

mrproper: clean

$(ROOT)$(MACDIR) $(ROOT)$(MANDIR)/man7:
	mkdir -p $@
