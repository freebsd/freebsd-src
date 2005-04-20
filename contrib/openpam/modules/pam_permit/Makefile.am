pkglibdir = @OPENPAM_MODULES_DIR@
INCLUDES = -I$(top_srcdir)/include

pkglib_LTLIBRARIES = pam_permit.la

pam_permit_la_SOURCES = pam_permit.c
pam_permit_la_LDFLAGS = -no-undefined -module -version-info @LIB_MAJ@
pam_permit_la_LIBADD = $(top_builddir)/lib/libpam.la
