# $Id: Makefile.am 21418 2007-07-05 13:55:37Z lha $

include $(top_srcdir)/Makefile.am.common

SUBDIRS = db kdc gss ldap can java

if ENABLE_SHARED
if HAVE_DLOPEN
SUBDIRS += plugin
endif
endif
