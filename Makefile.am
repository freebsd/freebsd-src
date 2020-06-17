ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = MAINT

SUBDIRS = src magic tests doc python

# This variable must have 'exec' in its name, in order to be installed
# by 'install-exec' target (instead of default 'install-data')
pkgconfigexecdir = $(libdir)/pkgconfig
pkgconfigexec_DATA = libmagic.pc
