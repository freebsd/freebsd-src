# $Id: Makefile.am 648 2013-03-05 17:54:27Z des $

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lib/libpam

noinst_PROGRAMS = openpam_dump_policy
openpam_dump_policy_SOURCES = openpam_dump_policy.c
openpam_dump_policy_LDADD = $(top_builddir)/lib/libpam/libpam.la
