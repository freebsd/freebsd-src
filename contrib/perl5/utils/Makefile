
PERL = ../miniperl
REALPERL = ../perl

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = c2ph.PL h2ph.PL h2xs.PL perlbug.PL perldoc.PL pl2pm.PL splain.PL perlcc.PL dprofpp.PL
plextract  = c2ph h2ph h2xs perlbug perldoc pl2pm splain perlcc dprofpp
plextractexe  = ./c2ph ./h2ph ./h2xs ./perlbug ./perldoc ./pl2pm ./splain ./perlcc ./dprofpp

all: $(plextract) 

compile: all $(plextract)
	$(REALPERL) -I../lib perlcc c2ph -o c2ph.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc h2ph -o h2ph.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc h2xs -o h2xs.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc perlbug -o perlbug.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc perldoc -o perldoc.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc pl2pm -o pl2pm.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc splain -o splain.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc perlcc -o perlcc.exe -v 10 -log ../compilelog;
	$(REALPERL) -I../lib perlcc dprofpp -o dprofpp.exe -v 10 -log ../compilelog;

$(plextract):
	$(PERL) -I../lib $@.PL

c2ph:		c2ph.PL ../config.sh

h2ph:		h2ph.PL ../config.sh

h2xs:		h2xs.PL ../config.sh

perlbug:	perlbug.PL ../config.sh ../patchlevel.h

perldoc:	perldoc.PL ../config.sh

pl2pm:		pl2pm.PL ../config.sh

splain:		splain.PL ../config.sh ../lib/diagnostics.pm

perlcc:		perlcc.PL ../config.sh

dprofpp:	dprofpp.PL ../config.sh

clean:

realclean:
	rm -rf $(plextract) pstruct $(plextractexe)
	rm -f ../t/_h2ph_pre.ph

clobber:	realclean

distclean:	clobber

veryclean:	distclean
	-rm -f *~ *.org
