# Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS =

AM_CXXFLAGS = $(WARN_CXXFLAGS) $(LFS_CXXFLAGS)

INCLUDES = -D_GNU_SOURCE \
	-I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../../include \
	-I$(srcdir)/../../elfcpp \
	-DLOCALEDIR="\"$(datadir)/locale\"" \
	@INCINTL@

TESTS = object_unittest

check_LIBRARIES = libgoldtest.a
libgoldtest_a_SOURCES = test.cc testmain.cc testfile.cc

LDADD = libgoldtest.a ../libgold.a

check_PROGRAMS = object_unittest

object_unittest_SOURCES = object_unittest.cc
