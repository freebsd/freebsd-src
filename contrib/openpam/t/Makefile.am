# $Id: Makefile.am 714 2013-08-19 15:30:21Z des $

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lib/libpam

noinst_HEADERS = t.h

# tests
TESTS =
TESTS += t_openpam_readword
TESTS += t_openpam_readlinev
check_PROGRAMS = $(TESTS)

# libt - common support code
check_LIBRARIES = libt.a
libt_a_SOURCES = t_main.c t_file.c

# link with libpam and libt
LDADD = libt.a $(top_builddir)/lib/libpam/libpam.la
