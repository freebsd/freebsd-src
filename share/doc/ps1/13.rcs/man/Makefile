#	Makefile,v 1.2 1993/08/02 17:51:17 mycroft Exp

DEST	      = $(DESTDIR)/usr/man/mann

MAKEFILE      = Makefile

PRINT	      = vtroff

SRCS	      = ci.1 \
		co.1 \
		ident.1 \
		merge.1 \
		rcs.1 \
		rcsdiff.1 \
		rcsintro.1 \
		rcsmerge.1 \
		rlog.1 \
		sccstorcs.1

SUFFIX	      = .1:s

all:;

clean:;

depend:;	@mkmf -f $(MAKEFILE)

install:;	@echo Installing $(SRCS) in $(DEST)
		@for i in $(SRCS); do cp $$i $(DEST)/`basename $$i .1`.n; done
		cp rcsfile.5 $(DEST)/rcsfile.n

print:		$(SRCS)
		@$(PRINT) -man $?
		@touch print

update:;
