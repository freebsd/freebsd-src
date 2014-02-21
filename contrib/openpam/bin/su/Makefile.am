# $Id: Makefile.am 648 2013-03-05 17:54:27Z des $

AM_CPPFLAGS = -I$(top_srcdir)/include

bin_PROGRAMS = su
su_SOURCES = su.c
su_LDADD = $(top_builddir)/lib/libpam/libpam.la

dist_man1_MANS = su.1
