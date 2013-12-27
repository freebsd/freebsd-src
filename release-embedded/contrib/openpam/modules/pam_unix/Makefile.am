# $Id: Makefile.am 690 2013-08-15 13:22:51Z des $

if CUSTOM_MODULES_DIR
moduledir = @OPENPAM_MODULES_DIR@
else
moduledir = $(libdir)
endif
AM_CPPFLAGS = -I$(top_srcdir)/include

module_LTLIBRARIES = pam_unix.la

pam_unix_la_SOURCES = pam_unix.c
pam_unix_la_LDFLAGS = -no-undefined -module -version-info @LIB_MAJ@ \
	-export-symbols-regex '^pam_sm_'
pam_unix_la_LIBADD = $(top_builddir)/lib/libpam/libpam.la @CRYPT_LIBS@
