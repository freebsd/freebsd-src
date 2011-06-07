# $Id: Makefile.am 21625 2007-07-17 07:48:26Z lha $

include $(top_srcdir)/Makefile.am.common

if do_roken_rename
ES = strtok_r.c snprintf.c strdup.c strupr.c getprogname.c
endif

AM_CPPFLAGS += $(ROKEN_RENAME)

YFLAGS = -d

include_HEADERS = sl.h

lib_LTLIBRARIES = libsl.la libss.la
libsl_la_LDFLAGS = -version-info 2:1:2
libss_la_LDFLAGS = -version-info 1:6:1

libsl_la_LIBADD = @LIB_readline@
libss_la_LIBADD = @LIB_readline@ @LIB_com_err@

dist_libsl_la_SOURCES = sl_locl.h sl.c roken_rename.h
nodist_libsl_la_SOURCES = $(ES)
dist_libss_la_SOURCES = $(dist_libsl_la_SOURCES) ss.c ss.h
nodist_libss_la_SOURCES = $(ES)

TESTS = test_sl
check_PROGRAMS = $(TESTS)	

# install these?

bin_PROGRAMS = mk_cmds
noinst_PROGRAMS = slc

mk_cmds_SOURCES = make_cmds.c make_cmds.h parse.y lex.l
mk_cmds_LDADD = libsl.la $(LDADD)

slc_SOURCES = slc-gram.y slc-lex.l slc.h

ssincludedir = $(includedir)/ss
ssinclude_HEADERS = ss.h

CLEANFILES = snprintf.c strtok_r.c strdup.c strupr.c getprogname.c

$(mk_cmds_OBJECTS): parse.h parse.c

LDADD =						\
	libsl.la				\
	$(LIB_roken)				\
	$(LEXLIB)

strtok_r.c:
	$(LN_S) $(srcdir)/../roken/strtok_r.c .
snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .
strdup.c:
	$(LN_S) $(srcdir)/../roken/strdup.c .
strupr.c:
	$(LN_S) $(srcdir)/../roken/strupr.c .
getprogname.c:
	$(LN_S) $(srcdir)/../roken/getprogname.c .

slc-lex.c: slc-gram.h
