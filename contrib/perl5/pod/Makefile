CONVERTERS = pod2html pod2latex pod2man pod2text checkpods

HTMLROOT = /	# Change this to fix cross-references in HTML
POD2HTML = pod2html \
	    --htmlroot=$(HTMLROOT) \
	    --podroot=.. --podpath=pod:lib:ext:vms \
	    --libpods=perlfunc:perlguts:perlvar:perlrun:perlop

all: $(CONVERTERS) man

converters: $(CONVERTERS)

PERL = ../miniperl
REALPERL = ../perl

POD = \
	perl.pod	\
	perldelta.pod	\
	perl5004delta.pod	\
	perldata.pod	\
	perlsyn.pod	\
	perlop.pod	\
	perlre.pod	\
	perlrun.pod	\
	perlfunc.pod	\
	perlopentut.pod	\
	perlvar.pod	\
	perlsub.pod	\
	perlmod.pod	\
	perlmodlib.pod	\
	perlmodinstall.pod	\
	perlform.pod	\
	perllocale.pod	\
	perlref.pod	\
	perlreftut.pod	\
	perldsc.pod	\
	perllol.pod	\
	perltoot.pod	\
	perlobj.pod	\
	perltie.pod	\
	perlbot.pod	\
	perlipc.pod	\
	perlthrtut.pod	\
	perldebug.pod	\
	perldiag.pod	\
	perlsec.pod	\
	perltrap.pod	\
	perlport.pod	\
	perlstyle.pod	\
	perlpod.pod	\
	perlbook.pod	\
	perlembed.pod	\
	perlapio.pod	\
	perlxs.pod	\
	perlxstut.pod	\
	perlguts.pod	\
	perlcall.pod	\
	perlhist.pod	\
	perlfaq.pod	\
	perlfaq1.pod	\
	perlfaq2.pod	\
	perlfaq3.pod	\
	perlfaq4.pod	\
	perlfaq5.pod	\
	perlfaq6.pod	\
	perlfaq7.pod	\
	perlfaq8.pod	\
	perlfaq9.pod	\
	perltoc.pod

MAN = \
	perl.man	\
	perldelta.man	\
	perl5004delta.man	\
	perldata.man	\
	perlsyn.man	\
	perlop.man	\
	perlre.man	\
	perlrun.man	\
	perlfunc.man	\
	perlopentut.man	\
	perlvar.man	\
	perlsub.man	\
	perlmod.man	\
	perlmodlib.man	\
	perlmodinstall.man	\
	perlform.man	\
	perllocale.man	\
	perlref.man	\
	perlreftut.man	\
	perldsc.man	\
	perllol.man	\
	perltoot.man	\
	perlobj.man	\
	perltie.man	\
	perlbot.man	\
	perlipc.man	\
	perlthrtut.man	\
	perldebug.man	\
	perldiag.man	\
	perlsec.man	\
	perltrap.man	\
	perlport.man	\
	perlstyle.man	\
	perlpod.man	\
	perlbook.man	\
	perlembed.man	\
	perlapio.man	\
	perlxs.man	\
	perlxstut.man	\
	perlguts.man	\
	perlcall.man	\
	perlhist.man	\
	perlfaq.man	\
	perlfaq1.man	\
	perlfaq2.man	\
	perlfaq3.man	\
	perlfaq4.man	\
	perlfaq5.man	\
	perlfaq6.man	\
	perlfaq7.man	\
	perlfaq8.man	\
	perlfaq9.man	\
	perltoc.man

HTML = \
	perl.html	\
	perldelta.html	\
	perl5004delta.html	\
	perldata.html	\
	perlsyn.html	\
	perlop.html	\
	perlre.html	\
	perlrun.html	\
	perlfunc.html	\
	perlopentut.html	\
	perlvar.html	\
	perlsub.html	\
	perlmod.html	\
	perlmodlib.html	\
	perlmodinstall.html	\
	perlform.html	\
	perllocale.html	\
	perlref.html	\
	perlreftut.html	\
	perldsc.html	\
	perllol.html	\
	perltoot.html	\
	perlobj.html	\
	perltie.html	\
	perlbot.html	\
	perlipc.html	\
	perlthrtut.html	\
	perldebug.html	\
	perldiag.html	\
	perlsec.html	\
	perltrap.html	\
	perlport.html	\
	perlstyle.html	\
	perlpod.html	\
	perlbook.html	\
	perlembed.html	\
	perlapio.html	\
	perlxs.html	\
	perlxstut.html	\
	perlguts.html	\
	perlcall.html	\
	perlhist.html	\
	perlfaq.html	\
	perlfaq1.html	\
	perlfaq2.html	\
	perlfaq3.html	\
	perlfaq4.html	\
	perlfaq5.html	\
	perlfaq6.html	\
	perlfaq7.html	\
	perlfaq8.html	\
	perlfaq9.html
# not perltoc.html

TEX = \
	perl.tex	\
	perldelta.tex	\
	perl5004delta.tex	\
	perldata.tex	\
	perlsyn.tex	\
	perlop.tex	\
	perlre.tex	\
	perlrun.tex	\
	perlfunc.tex	\
	perlopentut.tex	\
	perlvar.tex	\
	perlsub.tex	\
	perlmod.tex	\
	perlmodlib.tex	\
	perlmodinstall.tex	\
	perlform.tex	\
	perllocale.tex	\
	perlref.tex	\
	perlreftut.tex	\
	perlopentut.tex	\
	perldsc.tex	\
	perllol.tex	\
	perltoot.tex	\
	perlobj.tex	\
	perltie.tex	\
	perlbot.tex	\
	perlipc.tex	\
	perlthrtut.tex	\
	perldebug.tex	\
	perldiag.tex	\
	perlsec.tex	\
	perltrap.tex	\
	perlport.tex	\
	perlstyle.tex	\
	perlpod.tex	\
	perlbook.tex	\
	perlembed.tex	\
	perlapio.tex	\
	perlxs.tex	\
	perlxstut.tex	\
	perlguts.tex	\
	perlcall.tex	\
	perlhist.tex	\
	perlfaq.tex	\
	perlfaq1.tex	\
	perlfaq2.tex	\
	perlfaq3.tex	\
	perlfaq4.tex	\
	perlfaq5.tex	\
	perlfaq6.tex	\
	perlfaq7.tex	\
	perlfaq8.tex	\
	perlfaq9.tex	\
	perltoc.tex

man:	pod2man $(MAN)

html:	pod2html $(HTML)

tex:	pod2latex $(TEX)

toc:
	$(PERL) -I../lib buildtoc >perltoc.pod

.SUFFIXES: .pm .pod

.SUFFIXES: .man

.pm.man:     pod2man
	$(PERL) -I../lib pod2man $*.pm >$*.man

.pod.man:     pod2man
	$(PERL) -I../lib pod2man $*.pod >$*.man

.SUFFIXES: .html

.pm.html:    pod2html
	$(PERL) -I../lib $(POD2HTML) --infile=$*.pm --outfile=$*.html

.pod.html:    pod2html
	$(PERL) -I../lib $(POD2HTML) --infile=$*.pod --outfile=$*.html

.SUFFIXES: .tex

.pm.tex: pod2latex
	$(PERL) -I../lib pod2latex $*.pm

.pod.tex: pod2latex
	$(PERL) -I../lib pod2latex $*.pod

clean:
	rm -f $(MAN)
	rm -f $(HTML)
	rm -f $(TEX)
	rm -f pod2html-*cache
	rm -f *.aux *.log *.exe

realclean:	clean
	rm -f $(CONVERTERS)

distclean:	realclean

check:	checkpods
	@echo "checking..."; \
	$(PERL) -I../lib checkpods $(POD)

# Dependencies.
pod2latex:	pod2latex.PL ../lib/Config.pm
	$(PERL) -I../lib pod2latex.PL

pod2html:	pod2html.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2html.PL

pod2man:	pod2man.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2man.PL

pod2text:	pod2text.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2text.PL

checkpods:	checkpods.PL ../lib/Config.pm
	$(PERL) -I ../lib checkpods.PL

compile: all
	$(REALPERL) -I../lib ../utils/perlcc -regex 's/$$/.exe/' pod2latex pod2man pod2text checkpods -prog -verbose dcf -log ../compilelog;

	
