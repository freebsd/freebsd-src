# $Id: Makefile.am,v 1.24 2001/05/16 23:58:12 assar Exp $

include $(top_srcdir)/Makefile.am.common

if do_roken_rename
IS = -DROKEN_RENAME
ES = strtok_r.c snprintf.c strdup.c strupr.c
endif

INCLUDES += $(IS)

YFLAGS = -d

include_HEADERS = sl.h

lib_LTLIBRARIES = libsl.la libss.la
libsl_la_LDFLAGS = -version-info 1:2:1
libss_la_LDFLAGS = -version-info 1:4:1

libsl_la_LIBADD = @LIB_readline@
libss_la_LIBADD = @LIB_readline@

libsl_la_SOURCES = sl_locl.h sl.c $(ES)
libss_la_SOURCES = $(libsl_la_SOURCES) ss.c ss.h

# install these?

bin_PROGRAMS = mk_cmds

mk_cmds_SOURCES = make_cmds.c make_cmds.h parse.y lex.l $(ES)

ssincludedir = $(includedir)/ss
ssinclude_HEADERS = ss.h

CLEANFILES = lex.c parse.c parse.h snprintf.c strtok_r.c strdup.c strupr.c

$(mk_cmds_OBJECTS): parse.h

LDADD =						\
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
