# $Id: Makefile.am,v 1.14 1999/04/09 18:28:29 assar Exp $

include $(top_srcdir)/Makefile.am.common

YFLAGS = -d

include_HEADERS = sl.h

lib_LTLIBRARIES = libsl.la libss.la
libsl_la_LDFLAGS = -version-info 0:0:0
libss_la_LDFLAGS = -version-info 0:0:0

RENAME_SRC = roken_rename.h strtok_r.c snprintf.c

libsl_la_SOURCES = sl_locl.h sl.c
libss_la_SOURCES = $(libsl_la_SOURCES) ss.c ss.h

EXTRA_libsl_la_SOURCES = strtok_r.c snprintf.c roken_rename.h

# install these?

noinst_PROGRAMS = mk_cmds

mk_cmds_SOURCES = make_cmds.c make_cmds.h parse.y lex.l

RENAME_mk_cmds_SRC = roken_rename.h snprintf.c

EXTRA_mk_cmds_SOURCES = snprintf.c roken_rename.h

ssincludedir = $(includedir)/ss
ssinclude_HEADERS = ss.h

CLEANFILES = lex.c parse.c parse.h snprintf.c strtok_r.c

$(mk_cmds_OBJECTS): parse.h

LDADD = \
	$(LIB_roken) \
	$(LEXLIB)

strtok_r.c:
	$(LN_S) $(srcdir)/../roken/strtok_r.c .
snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .
