## Process this file with automake to produce Makefile.in

# Package: am-utils
# Level: Makefile for fixmount/ directory
# Author: Erez Zadok

sbin_PROGRAMS = fixmount

# man pages
man_MANS = fixmount.8

# headers this depends on, not to be installed
#noinst_HEADERS =

fixmount_SOURCES = fixmount.c
EXTRA_fixmount_OBJECTS = check_mount.o
LDADD = $(EXTRA_fixmount_OBJECTS) ../libamu/libamu.la

AM_CPPFLAGS = -I$(top_srcdir)/include

# allow users to add their own flags via "configure --enable-am-flags=ARG"
AMU_CFLAGS = @AMU_CFLAGS@
AM_CFLAGS = $(AMU_CFLAGS)

# additional files to distribute and clean
EXTRA_DIST = $(man_MANS)
DISTCLEANFILES = check_mount.c

# dependencies
$(PROGRAMS): $(LDADD)
$(fixmount_OBJECTS) $(EXTRA_fixmount_OBJECTS):	\
	../config.h				\
	../aux_conf.h				\
	$(top_srcdir)/include/am_compat.h	\
	$(top_srcdir)/include/am_defs.h		\
	$(top_srcdir)/include/am_utils.h	\
	$(top_srcdir)/include/am_xdr_func.h	\
	$(top_srcdir)/include/amq_defs.h	\
	@AMU_NFS_PROT_HEADER@			\
	$(noinst_HEADERS)
