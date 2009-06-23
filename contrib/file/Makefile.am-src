MAGIC = $(pkgdatadir)/magic
lib_LTLIBRARIES = libmagic.la
include_HEADERS = magic.h

bin_PROGRAMS = file

AM_CPPFLAGS = -DMAGIC='"$(MAGIC)"'
AM_CFLAGS = @WARNINGS@

libmagic_la_SOURCES = magic.c apprentice.c softmagic.c ascmagic.c \
	encoding.c compress.c is_tar.c readelf.c print.c fsmagic.c \
	funcs.c file.h names.h patchlevel.h readelf.h tar.h apptype.c \
	file_opts.h elfclass.h mygetopt.h cdf.c cdf_time.c readcdf.c cdf.h
libmagic_la_LDFLAGS = -no-undefined -version-info 1:0:0
libmagic_la_LIBADD = $(LTLIBOBJS)

file_SOURCES = file.c
file_LDADD = libmagic.la
