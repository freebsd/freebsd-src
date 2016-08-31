## Process this file with automake to produce Makefile.in

# Package: am-utils
# Level: Makefile for mk-amd-map/ directory
# Author: Erez Zadok

sbin_PROGRAMS = mk-amd-map

# man pages
man_MANS = mk-amd-map.8

# headers this depends on, not to be installed
#noinst_HEADERS =

mk_amd_map_SOURCES = mk-amd-map.c

LDADD = ../libamu/libamu.la

AM_CPPFLAGS = -I$(top_srcdir)/include

# allow users to add their own flags via "configure --enable-am-flags=ARG"
AMU_CFLAGS = @AMU_CFLAGS@
AM_CFLAGS = $(AMU_CFLAGS)

# additional files to distribute and clean
EXTRA_DIST = $(man_MANS)

# dependencies
$(PROGRAMS): $(LDADD)
$(mk_amd_map_OBJECTS):				\
	../config.h				\
	../aux_conf.h				\
	$(top_srcdir)/include/am_compat.h	\
	$(top_srcdir)/include/am_defs.h		\
	$(top_srcdir)/include/am_utils.h	\
	$(top_srcdir)/include/am_xdr_func.h	\
	$(top_srcdir)/include/amq_defs.h	\
	@AMU_NFS_PROT_HEADER@			\
	$(noinst_HEADERS)
