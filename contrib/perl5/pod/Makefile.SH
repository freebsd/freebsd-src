case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

if test -d pod; then
  cd pod || exit 1
fi
POD=`echo *.pod`
MAN=`echo $POD|sed 's/\.pod/\.man/g'`
HTML=`echo $POD|sed 's/perltoc.pod//'|sed 's/\.pod/\.html/g'`
TEX=`echo $POD|sed 's/\.pod/\.tex/g'`

echo "Extracting pod/Makefile (with variable substitutions)"
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.

$spitshell >Makefile <<!GROK!THIS!
# pod/Makefile
# This file is derived from pod/Makefile.SH.  Any changes made here will
# be lost the next time you run Configure.

POD = $POD

MAN = $MAN

# no perltoc.html
HTML = $HTML

TEX  = $TEX  

!GROK!THIS!

## In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

CONVERTERS = pod2html pod2latex pod2man pod2text checkpods \
		pod2usage podchecker podselect

HTMLROOT = /	# Change this to fix cross-references in HTML
POD2HTML = pod2html \
	    --htmlroot=$(HTMLROOT) \
	    --podroot=.. --podpath=pod:lib:ext:vms \
	    --libpods=perlfunc:perlguts:perlvar:perlrun:perlop

PERL = ../miniperl
PERLILIB = $(PERL) -I../lib
REALPERL = ../perl

all: $(CONVERTERS) man

converters: $(CONVERTERS)

regen_pods: perlmodlib.pod toc

buildtoc:	buildtoc.PL perl.pod ../MANIFEST
	$(PERLILIB) buildtoc.PL

perltoc.pod:	buildtoc

man:	pod2man $(MAN)

html:	pod2html $(HTML)

tex:	pod2latex $(TEX)

toc:	buildtoc
	$(PERLILIB) buildtoc

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

veryclean:	distclean
	-rm -f *~ *.orig

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

pod2usage:	pod2usage.PL ../lib/Config.pm
	$(PERL) -I ../lib pod2usage.PL

podchecker:	podchecker.PL ../lib/Config.pm
	$(PERL) -I ../lib podchecker.PL

podselect:	podselect.PL ../lib/Config.pm
	$(PERL) -I ../lib podselect.PL

perlmodlib.pod:	$(PERL) perlmodlib.PL ../mv-if-diff
	rm -f perlmodlib.tmp
	$(PERL) -I ../lib perlmodlib.PL
	sh ../mv-if-diff perlmodlib.tmp perlmodlib.pod

compile: all
	$(REALPERL) -I../lib ../utils/perlcc -o pod2latex.exe pod2latex -log ../compilelog
	$(REALPERL) -I../lib ../utils/perlcc -o pod2man.exe pod2man -log ../compilelog
	$(REALPERL) -I../lib ../utils/perlcc -o pod2text.exe pod2text -log ../compilelog
	$(REALPERL) -I../lib ../utils/perlcc -o checkpods.exe checkpods -log ../compilelog

!NO!SUBS!
