# $Id: Makefile.am,v 1.24 2000/08/16 11:24:54 assar Exp $

include $(top_srcdir)/Makefile.am.common

YFLAGS = -d

lib_LTLIBRARIES = libcom_err.la
libcom_err_la_LDFLAGS = -version-info 1:1:0

bin_PROGRAMS = compile_et

include_HEADERS = com_err.h com_right.h

compile_et_SOURCES = compile_et.c compile_et.h parse.y lex.l

libcom_err_la_SOURCES = error.c com_err.c roken_rename.h

CLEANFILES = lex.c parse.c parse.h

$(compile_et_OBJECTS): parse.h

compile_et_LDADD = \
	$(LIB_roken) \
	$(LEXLIB)
