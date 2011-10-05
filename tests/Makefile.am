# $Id$

include $(top_srcdir)/Makefile.am.common

SUBDIRS = bin db kdc gss ldap can java

if ENABLE_SHARED
if HAVE_DLOPEN
SUBDIRS += plugin
endif
endif

EXTRA_DIST = NTMakefile
