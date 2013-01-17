# $Id: Makefile.am 429 2010-03-09 17:51:29Z des $

pkglibdir = @OPENPAM_MODULES_DIR@
INCLUDES = -I$(top_srcdir)/include

if WITH_PAM_UNIX
pkglib_LTLIBRARIES = pam_unix.la

pam_unix_la_SOURCES = pam_unix.c
pam_unix_la_LDFLAGS = -no-undefined -module -version-info @LIB_MAJ@
pam_unix_la_LIBADD = $(top_builddir)/lib/libpam.la @CRYPT_LIBS@
endif
