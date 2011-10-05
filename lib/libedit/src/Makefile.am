
BUILT_SOURCES = vi.h emacs.h common.h fcns.h help.h fcns.c help.c
if WIDECHAR
BUILT_SOURCES += tokenizern.c historyn.c
endif

AHDR= vi.h emacs.h common.h 
ASRC= $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c

vi.h: Makefile $(srcdir)/vi.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/vi.c > $@

emacs.h: Makefile $(srcdir)/emacs.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/emacs.c > $@

common.h: Makefile $(srcdir)/common.c
	AWK=$(AWK) sh $(srcdir)/makelist -h $(srcdir)/common.c > $@

fcns.h: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fh $(AHDR) > $@

help.h: Makefile $(ASRC)
	AWK=$(AWK) sh $(srcdir)/makelist -bh $(ASRC) > $@

fcns.c: Makefile $(AHDR)
	AWK=$(AWK) sh $(srcdir)/makelist -fc $(AHDR) > $@

help.c: Makefile $(ASRC)
	AWK=$(AWK) sh $(srcdir)/makelist -bc $(ASRC) > $@

tokenizern.c: Makefile $(srcdir)/tokenizer.c
	AWK=$(AWK) sh $(srcdir)/makelist -n $(srcdir)/tokenizer.c > $@

historyn.c: Makefile $(srcdir)/history.c
	AWK=$(AWK) sh $(srcdir)/makelist -n $(srcdir)/history.c > $@

CLEANFILES = $(BUILT_SOURCES)

lib_LTLIBRARIES    = libheimedit.la
libheimedit_la_SOURCES = chared.c common.c el.c emacs.c hist.c key.c map.c chartype.c parse.c \
							prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c \
							fgetln.c strlcat.c strlcpy.c unvis.c vis.c wcsdup.c tokenizer.c \
							history.c filecomplete.c readline.c chared.h el.h hist.h \
							histedit.h key.h map.h chartype.h parse.h prompt.h read.h refresh.h \
							search.h sig.h sys.h el_term.h tty.h vis.h filecomplete.h \
							editline/readline.h
if WIDECHAR
libheimedit_la_SOURCES += eln.c
endif

EXTRA_DIST = makelist shlib_version
#nobase_include_HEADERS = histedit.h editline/readline.h
EXTRA_DIST += histedit.h editline/readline.h

nodist_libheimedit_la_SOURCES = $(BUILT_SOURCES)

libheimedit_la_LDFLAGS = -no-undefined -version-info $(LT_VERSION)

