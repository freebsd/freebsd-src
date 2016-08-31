## Process this file with automake to produce Makefile.in

# Package: am-utils
# Level: Makefile for wire-test/ directory
# Author: Erez Zadok

sbin_PROGRAMS = wire-test

# man pages
man_MANS = wire-test.8

# headers this depends on, not to be installed
#noinst_HEADERS =

wire_test_SOURCES = wire-test.c

LDADD = ../libamu/libamu.la

AM_CPPFLAGS = -I$(top_srcdir)/include

# allow users to add their own flags via "configure --enable-am-flags=ARG"
AMU_CFLAGS = @AMU_CFLAGS@
AM_CFLAGS = $(AMU_CFLAGS)

# additional files to distribute and clean
EXTRA_DIST = $(man_MANS)

# dependencies
$(PROGRAMS): $(LDADD)
$(wire_test_OBJECTS):				\
	../config.h				\
	../aux_conf.h				\
	$(top_srcdir)/include/am_compat.h	\
	$(top_srcdir)/include/am_defs.h		\
	$(top_srcdir)/include/am_utils.h	\
	$(top_srcdir)/include/am_xdr_func.h	\
	$(top_srcdir)/include/amq_defs.h	\
	@AMU_NFS_PROT_HEADER@			\
	$(noinst_HEADERS)
