case $CONFIGDOTSH in
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
*/Makefile.SH) cd `expr X$0 : 'X\(.*\)/'` ;;
Makefile.SH) ;;
*) case `pwd` in
   */x2p) ;;
   *) if test -d x2p; then cd x2p
      else echo "Can't figure out where to write output."; exit 1
	  fi;;
   esac;;
esac

echo "Extracting x2p/Makefile (with variable substitutions)"
rm -f Makefile
cat >Makefile <<!GROK!THIS!
# $RCSfile: Makefile.SH,v $$Revision: 4.1 $$Date: 92/08/07 18:29:07 $
#
# $Log:	Makefile.SH,v $

CC = $cc
BYACC = $byacc
LDFLAGS = $ldflags
# XXX Perl malloc temporarily unusable (declaration collisions with stdlib.h)
#mallocsrc = $mallocsrc
#mallocobj = $mallocobj
shellflags = $shellflags

libs = $libs

$make_set_make
# grrr
SHELL = $sh

# These variables may need to be manually set for non-Unix systems.
AR = $ar
EXE_EXT = $_ext
LIB_EXT = $_a
OBJ_EXT = $_o
PATH_SEP = $p_

FIRSTMAKEFILE = $firstmakefile

# how to tr(anslate) newlines

TRNL = '$trnl'

.SUFFIXES: .c \$(OBJ_EXT)

!GROK!THIS!

cat >>Makefile <<'!NO!SUBS!'

REALPERL = ../perl
CCCMD = `sh $(shellflags) cflags $@`

public = a2p s2p find2perl

private = 

manpages = a2p.man s2p.man

util =

sh = Makefile.SH cflags.SH
shextract = Makefile cflags

pl = find2perl.PL s2p.PL
plextract = find2perl s2p
plexe = find2perl.exe s2p.exe
plc   = find2perl.c s2p.c
plm   = a2p.loadmap

addedbyconf = $(shextract) $(plextract)

h = EXTERN.h INTERN.h ../config.h ../handy.h hash.h a2p.h str.h util.h

c = hash.c $(mallocsrc) str.c util.c walk.c

obj = hash$(OBJ_EXT) $(mallocobj) str$(OBJ_EXT) util$(OBJ_EXT) walk$(OBJ_EXT)

lintflags = -phbvxac


.c$(OBJ_EXT):
	$(CCCMD) -DPERL_FOR_X2P $*.c

all: $(public) $(private) $(util)
	@echo " "

compile: all
	$(REALPERL) -I../lib ../utils/perlcc -regex 's/$$/.exe/' $(plextract) -prog -verbose dcf -log ../compilelog;  

a2p: $(obj) a2p$(OBJ_EXT)
	$(CC) -o a2p $(LDFLAGS) $(obj) a2p$(OBJ_EXT) $(libs)

# I now supply a2p.c with the kits, so the following section is
# used only if you force byacc to run by saying
# make  run_byacc

run_byacc:	FORCE
	@ echo Expect many shift/reduce and reduce/reduce conflicts
	$(BYACC) a2p.y
	rm -f a2p.c
	mv y.tab.c a2p.c

FORCE:

# We don't want to regenerate a2p.c, but it might appear out-of-date
# after a patch is applied or a new distribution is made.
a2p.c: a2p.y
	-@sh -c true

a2p$(OBJ_EXT): a2p.c a2py.c a2p.h EXTERN.h util.h INTERN.h \
		../handy.h ../config.h str.h hash.h
	$(CCCMD) a2p.c

clean:
	rm -f a2p *$(OBJ_EXT) $(plexe) $(plc) $(plm)

realclean: clean
	rm -f core $(addedbyconf) all malloc.c
	rm -f $(FIRSTMAKEFILE) $(FIRSTMAKEFILE).old

veryclean: realclean
	rm -f *~ *.orig

# The following lint has practically everything turned on.  Unfortunately,
# you have to wade through a lot of mumbo jumbo that can't be suppressed.
# If the source file has a /*NOSTRICT*/ somewhere, ignore the lint message
# for that spot.

lint:
	lint $(lintflags) $(defs) $(c) > a2p.fuzz

depend: $(mallocsrc) ../makedepend
	sh ../makedepend MAKE=$(MAKE)

clist:
	echo $(c) | tr ' ' $(TRNL) >.clist

hlist:
	echo $(h) | tr ' ' $(TRNL) >.hlist

shlist:
	echo $(sh) | tr ' ' $(TRNL) >.shlist

# These should be automatically generated

$(plextract):
	../miniperl -I../lib $@.PL

malloc.c: ../malloc.c
	rm -f malloc.c
	sed <../malloc.c >malloc.c \
	    -e 's/"EXTERN.h"/"..\/EXTERN.h"/' \
	    -e 's/"perl.h"/"..\/perl.h"/' \
	    -e 's/my_exit/exit/' \
	    -e 's/MUTEX_[A-Z_]*(&PL_malloc_mutex);//'

# AUTOMATICALLY GENERATED MAKE DEPENDENCIES--PUT NOTHING BELOW THIS LINE
$(obj):
	@ echo "You haven't done a "'"make depend" yet!'; exit 1
makedepend: depend
!NO!SUBS!
$eunicefix Makefile
case `pwd` in
*SH)
    $rm -f ../Makefile
    $ln Makefile ../Makefile
    ;;
esac
rm -f $firstmakefile
