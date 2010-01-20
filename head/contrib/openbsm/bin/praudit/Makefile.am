#
# $P4: //depot/projects/trustedbsd/openbsm/bin/praudit/Makefile.am#3 $
#

if USE_NATIVE_INCLUDES
INCLUDES = -I$(top_builddir) -I$(top_srcdir)
else
INCLUDES = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/sys
endif

sbin_PROGRAMS = praudit
praudit_SOURCES = praudit.c
praudit_LDADD = $(top_builddir)/libbsm/libbsm.la
man1_MANS = praudit.1
