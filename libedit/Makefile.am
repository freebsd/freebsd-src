## $NetBSD: Makefile.am,v 1.3 2010/01/05 07:15:58 lukem Exp $

noinst_LTLIBRARIES = libedit.la

libedit_la_SOURCES = \
	chared.c \
	common.c \
	el.c \
	emacs.c \
	hist.c \
	history.c \
	key.c \
	map.c \
	parse.c \
	prompt.c \
	read.c \
	refresh.c \
	search.c \
	sig.c \
	term.c \
	tokenizer.c \
	tty.c \
	vi.c

libedit_la_CPPFLAGS = \
	-I. \
	-I$(srcdir) \
	-I$(top_srcdir)/libnetbsd \
	-I$(top_srcdir) \
	-I$(top_builddir)

libedit_la_LIBADD = \
	$(top_builddir)/libnetbsd/libnetbsd.la

generated_files = \
	common.h \
	emacs.h \
	fcns.c \
	fcns.h \
	help.c \
	help.h\
	vi.h

nodist_libedit_la_SOURCES = \
	$(generated_files)

BUILT_SOURCES = \
	$(generated_files)

CLEANFILES = \
	$(generated_files)



vi.h: vi.c
	@echo "Create $@"
	$(SHELL) ./makelist -h $(srcdir)/vi.c > $@

emacs.h: emacs.c
	@echo "Create $@"
	$(SHELL) ./makelist -h $(srcdir)/emacs.c > $@

common.h: common.c
	@echo "Create $@"
	$(SHELL) ./makelist -h $(srcdir)/common.c > $@

fcns.h: vi.h emacs.h common.h
	@echo "Create $@"
	$(SHELL) ./makelist -fh vi.h emacs.h common.h > $@

fcns.c: vi.h emacs.h common.h fcns.h
	@echo "Create $@"
	$(SHELL) ./makelist -fc vi.h emacs.h common.h > $@

help.c: vi.c emacs.c common.c
	@echo "Create $@"
	$(SHELL) ./makelist -bc $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c > $@

help.h: vi.c emacs.c common.c
	@echo "Create $@"
	$(SHELL) ./makelist -bh $(srcdir)/vi.c $(srcdir)/emacs.c $(srcdir)/common.c > $@

EXTRA_DIST = \
	chared.h \
	editline.3 \
	editrc.5 \
	el.h \
	filecomplete.c \
	filecomplete.h \
	hist.h \
	histedit.h \
	key.h \
	map.h \
	parse.h \
	prompt.h \
	read.h \
	readline.c \
	readline/readline.h \
	refresh.h \
	search.h \
	sig.h \
	sys.h \
	term.h \
	tty.h
