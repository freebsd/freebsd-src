AUTOMAKE_OPTIONS = foreign # ansi2knr

#lib_LIBRARIES = libares.a
noinst_LIBRARIES = libares.a
libares_a_SOURCES = arlib.c arplib.h
#include_HEADERS = arlib.h
noinst_HEADERS = arlib.h

noinst_PROGRAMS = example
example_SOURCES = sample.c
example_LDADD = libares.a
#man_MANS = arlib.3
#EXTRA_DIST = $(man_MANS) UNSHAR.HDR
noinst_MANS = arlib.3
EXTRA_DIST = $(noinst_MANS) UNSHAR.HDR
