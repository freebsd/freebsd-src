# $Id: Makefile.am 763 2014-02-26 16:29:16Z des $

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lib/libpam

noinst_HEADERS = t.h

# tests
TESTS =
TESTS += t_openpam_ctype
TESTS += t_openpam_readword
TESTS += t_openpam_readlinev
check_PROGRAMS = $(TESTS)

# libt - common support code
check_LIBRARIES = libt.a
libt_a_SOURCES = t_main.c t_file.c

# link with libpam and libt
LDADD = libt.a $(top_builddir)/lib/libpam/libpam.la
