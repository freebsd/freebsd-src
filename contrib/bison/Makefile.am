## Process this file with automake to produce Makefile.in -*-Makefile-*-
AUTOMAKE_OPTIONS = 1.4 ansi2knr

bin_PROGRAMS = bison

bison_SOURCES = LR0.c allocate.c closure.c conflicts.c derives.c	\
    files.c getargs.c gram.c lalr.c lex.c main.c nullable.c output.c	\
    print.c reader.c reduce.c symtab.c warshall.c getopt.c getopt1.c

EXTRA_bison_SOURCES = vmsgetargs.c

bison_LDADD = @INTLLIBS@ @ALLOCA@
INCLUDES = -I../intl -I$(top_srcdir)/intl

noinst_HEADERS = alloc.h files.h gram.h lex.h machine.h state.h	\
 symtab.h system.h types.h getopt.h

data_DATA = bison.simple bison.hairy

EXTRA_DIST = bison.s1 bison.hairy build.com bison.cld vmshlp.mar

bison.simple: bison.s1 Makefile
	-rm -f $@
	sed -e "/^#line/ s|bison|$(datadir)/bison|" -e "s/@bison_version@/$(VERSION)/" < $(srcdir)/bison.s1 > $@-tmp
	mv $@-tmp $@

DISTCLEANFILES = bison.simple
