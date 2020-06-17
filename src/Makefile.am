AM_YFLAGS = -d
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
LIBS = @LIBS@

m4 = @M4@

bin_PROGRAMS = flex
if ENABLE_BOOTSTRAP
noinst_PROGRAMS = stage1flex
endif

if ENABLE_LIBFL
lib_LTLIBRARIES = libfl.la
endif
libfl_la_SOURCES = \
	libmain.c \
	libyywrap.c
libfl_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@

stage1flex_SOURCES = \
	scan.l \
	$(COMMON_SOURCES)

if CROSS
stage1flex_LDADD =
stage1flex_SOURCES += \
		      ../lib/malloc.c \
		      ../lib/realloc.c
stage1flex_LINK = $(LIBTOOL) --tag=CC --mode=link $(CC_FOR_BUILD) \
		  $(CFLAGS_FOR_BUILD) $(LDFLAGS_FOR_BUILD) -o $@

$(stage1flex_OBJECTS): CC=$(CC_FOR_BUILD)
$(stage1flex_OBJECTS): CFLAGS=$(CFLAGS_FOR_BUILD)
$(stage1flex_OBJECTS): CPP=$(CPP_FOR_BUILD)
$(stage1flex_OBJECTS): CPPFLAGS=$(CPPFLAGS_FOR_BUILD)
$(stage1flex_OBJECTS): LDFLAGS=$(LDFLAGS_FOR_BUILD)
else
stage1flex_LDADD = $(LDADD)
stage1flex_LINK = $(LINK)
stage1flex_CFLAGS = $(AM_CFLAGS)
endif

flex_SOURCES = \
	$(COMMON_SOURCES)

nodist_flex_SOURCES = \
	stage1scan.c

flex_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS)

COMMON_SOURCES = \
	buf.c \
	ccl.c \
	dfa.c \
	ecs.c \
	filter.c \
	flexdef.h \
	flexint.h \
	gen.c \
	main.c \
	misc.c \
	nfa.c \
	options.c \
	options.h \
	parse.y \
	regex.c \
	scanflags.c \
	scanopt.c \
	scanopt.h \
	skel.c \
	sym.c \
	tables.c \
	tables.h \
	tables_shared.c \
	tables_shared.h \
	tblcmp.c \
	version.h \
	yylex.c

LDADD = $(LIBOBJS) @LIBINTL@

include_HEADERS = \
	FlexLexer.h

EXTRA_DIST = \
	flex.skl \
	mkskel.sh \
	gettext.h

CLEANFILES = stage1scan.c stage1flex$(EXEEXT)

MAINTAINERCLEANFILES = skel.c

skel.c: flex.skl mkskel.sh flexint.h tables_shared.h tables_shared.c
	$(SHELL) $(srcdir)/mkskel.sh $(srcdir) $(m4) $(VERSION) > $@.tmp
	mv $@.tmp $@

if ENABLE_BOOTSTRAP
stage1scan.c: scan.l stage1flex$(EXEEXT)
	./stage1flex$(EXEEXT) $(AM_LFLAGS) $(LFLAGS) -o $@ $(srcdir)/scan.l
else
stage1scan.c: scan.c
	sed 's|^\(#line .*\)"'`printf %s $< | sed 's|[][\\\\.*]|\\\\&|g'`'"|\1"$@"|g' $< > $@
endif

dist-hook: scan.l flex$(EXEEXT)
	chmod u+w $(distdir)/scan.c && \
	./flex$(EXEEXT) -o scan.c $< && \
	mv scan.c $(distdir)

# make needs to be told to make parse.h so that parallelized runs will
# not fail.

stage1flex-main.$(OBJEXT): parse.h
flex-main.$(OBJEXT): parse.h

stage1flex-yylex.$(OBJEXT): parse.h
flex-yylex.$(OBJEXT): parse.h

stage1flex-scan.$(OBJEXT): parse.h
flex-stage1scan.$(OBJEXT): parse.h

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

indent: $(top_srcdir)/.indent.pro
	cd $(top_srcdir) && \
	for f in $(indentfiles); do \
		f=src/$$f; \
		echo indenting $$f; \
		INDENT_PROFILE=.indent.pro $(INDENT) <$$f >/dev/null && \
		INDENT_PROFILE=.indent.pro $(INDENT) $$f || \
		echo $$f FAILED to indent; \
	done;

.PHONY: indent
