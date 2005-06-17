# $P4: //depot/projects/openpam/modules/pam_deny/Makefile.am#2 $

pkglibdir = @OPENPAM_MODULES_DIR@
INCLUDES = -I$(top_srcdir)/include

pkglib_LTLIBRARIES = pam_deny.la

pam_deny_la_SOURCES = pam_deny.c
pam_deny_la_LDFLAGS = -no-undefined -module -version-info @LIB_MAJ@
pam_deny_la_LIBADD = $(top_builddir)/lib/libpam.la
