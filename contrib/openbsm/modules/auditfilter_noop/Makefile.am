##
## $P4: //depot/projects/trustedbsd/openbsm/modules/auditfilter_noop/Makefile.am#4 $
##

if USE_NATIVE_INCLUDES
INCLUDES = -I$(top_builddir) -I$(top_srcdir)
else
INCLUDES = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/sys
endif

lib_LTLIBRARIES = auditfilter_noop.la

auditfilter_noop_la_SOURCE = auditfilter_noop.c
auditfilter_noop_la_LDFLAGS = -module
