# This file is part of flex.

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:

# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.

# Neither the name of the University nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.

# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE.

# Notes on building:

# Possible values for DEFS:
#
# By default, flex generates 8-bit scanners when using table compression,
# and 7-bit scanners when using uncompressed tables (-f or -F options).
# For flex to always generate 8-bit scanners, add "-DDEFAULT_CSIZE=256"
# to DEFS.
#
# For Vax/VMS, add "-DVMS" to DEFS.
#
# For MS-DOS, add "-DMS_DOS" to DEFS.  See the directory MISC/MSDOS for
# additional info.

AM_YFLAGS = -d
ACLOCAL_AMFLAGS = -I m4
m4 = @M4@
indent = @INDENT@

bin_PROGRAMS = flex
lib_LIBRARIES = \
	libfl.a \
	libfl_pic.a

flex_SOURCES = \
	ccl.c \
	dfa.c \
	ecs.c \
	scanflags.c \
	gen.c \
	main.c \
	misc.c \
	nfa.c \
	parse.y \
	scan.l \
	skel.c \
	sym.c \
	tblcmp.c \
	yylex.c \
	options.c \
	scanopt.c \
	buf.c \
	tables.c \
	tables_shared.c \
	filter.c \
	regex.c


LDADD = lib/libcompat.a

libfl_a_SOURCES = \
	libmain.c \
	libyywrap.c

libfl_pic_a_SOURCES = \
	libmain.c \
	libyywrap.c

libfl_pic_a_CFLAGS =  \
	-fPIC         \
	$(AM_CFLAGS)

noinst_HEADERS = \
	flexdef.h \
	flexint.h \
	version.h \
	options.h \
	scanopt.h \
	tables.h \
	tables_shared.h

include_HEADERS = \
	FlexLexer.h

dist_doc_DATA = \
	AUTHORS \
	COPYING \
	NEWS \
	ONEWS \
	README \
	README.cvs \
	TODO

EXTRA_DIST = \
	.indent.pro \
	ABOUT-NLS \
	INSTALL \
	autogen.sh \
	flex.skl \
	mkskel.sh \
	config.rpath \
	gettext.h

BUILT_SOURCES = \
	skel.c

SUBDIRS = \
	lib \
	. \
	doc \
	examples \
	po \
	tools \
	tests

localedir = $(datadir)/locale
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -I$(top_srcdir)/intl
LIBS = @LIBINTL@ @LIBS@

skel.c: flex.skl mkskel.sh flexint.h tables_shared.h
	sed 's/m4_/m4postproc_/g; s/m4preproc_/m4_/g' $(srcdir)/flex.skl | $(m4) -P -DFLEX_MAJOR_VERSION=`echo $(VERSION)|cut -f 1 -d .` -DFLEX_MINOR_VERSION=`echo $(VERSION)|cut -f 2 -d .` -DFLEX_SUBMINOR_VERSION=`echo $(VERSION)|cut -f 3 -d .` | sed 's/m4postproc_/m4_/g' | $(SHELL) $(srcdir)/mkskel.sh  >skel.c

# Explicitly describe dependencies.
# You can recreate this with `gcc -I. -MM *.c'
buf.o: buf.c flexdef.h flexint.h
ccl.o: ccl.c flexdef.h flexint.h
dfa.o: dfa.c flexdef.h flexint.h tables.h tables_shared.h
ecs.o: ecs.c flexdef.h flexint.h
scanflags.o: scanflags.c flexdef.h flexint.h
gen.o: gen.c flexdef.h flexint.h tables.h tables_shared.h
libmain.o: libmain.c
libyywrap.o: libyywrap.c
main.o: main.c flexdef.h flexint.h version.h options.h scanopt.h \
 tables.h tables_shared.h
misc.o: misc.c flexdef.h flexint.h tables.h tables_shared.h
nfa.o: nfa.c flexdef.h flexint.h
options.o: options.c options.h scanopt.h flexdef.h flexint.h
parse.o: parse.c flexdef.h flexint.h tables.h tables_shared.h
scan.o: scan.c flexdef.h flexint.h parse.h
scanopt.o: scanopt.c flexdef.h flexint.h scanopt.h
skel.o: skel.c flexdef.h flexint.h
sym.o: sym.c flexdef.h flexint.h
tables.o: tables.c flexdef.h flexint.h tables.h tables_shared.h
tables_shared.o: tables_shared.c flexdef.h flexint.h tables.h \
 tables_shared.h
tblcmp.o: tblcmp.c flexdef.h flexint.h
yylex.o: yylex.c flexdef.h flexint.h parse.h
filter.o: filter.c flexdef.h flexint.h

# Create the ChangeLog, but only if we're inside a git working directory

ChangeLog: $(srcdir)/tools/git2cl
	if [ -d $(srcdir)/.git ] ; then \
		$(srcdir)/tools/git2cl > $@ \
	; fi

# Run GNU indent on sources. Don't run this unless all the sources compile cleanly.
#
# Whole idea:
#   1. Check for .indent.pro, otherwise indent will use unknown
#      settings, or worse, the GNU defaults.)
#   2. Check that this is GNU indent.
#   3. Make sure to process only the NON-generated .c and .h files.
#   4. Run indent twice per file. The first time is a test.
#      Otherwise, indent overwrites your file even if it fails!
indentfiles = \
	buf.c \
	ccl.c \
	dfa.c \
	ecs.c \
	scanflags.c \
	filter.c \
	flexdef.h \
	gen.c \
	libmain.c \
	libyywrap.c \
	main.c \
	misc.c \
	nfa.c \
	options.c \
	options.h \
	regex.c \
	scanopt.c \
	scanopt.h \
	sym.c \
	tables.c \
	tables.h \
	tables_shared.c \
	tables_shared.h \
	tblcmp.c

indent:
	if [ -f .indent.pro ] ; then \
	for f in $(indentfiles);\
	do\
		echo indenting $$f ;\
		$(indent) < $$f >/dev/null && indent $$f || echo $$f FAILED to indent ;\
	done \
	fi

install-exec-hook:
	cd $(DESTDIR)/$(bindir) && \
		$(LN_S) -f flex$(EXEEXT) flex++$(EXEEXT)

.PHONY: ChangeLog tags indent
