#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
noinst_PROGRAMS = @TESTDCF@ @DCFD@
EXTRA_PROGRAMS = testdcf dcfd
INCLUDES = -I$(top_srcdir)/include
ETAGS_ARGS = Makefile.am
DISTCLEANFILES = $(EXTRA_PROGRAMS)
#EXTRA_DIST= TAGS

check-local: dcfd
	./dcfd -Y
