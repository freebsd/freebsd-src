## $NetBSD: Makefile.am,v 1.2 2010/01/04 06:24:20 lukem Exp $

noinst_LTLIBRARIES = libnetbsd.la

libnetbsd_la_SOURCES =


CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_builddir)

libnetbsd_la_LIBADD = $(LTLIBOBJS)

EXTRA_DIST = \
	fseeko.c \
	ftpglob.h \
	ftpvis.h
