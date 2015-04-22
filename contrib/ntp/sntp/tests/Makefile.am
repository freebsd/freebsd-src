NULL =
BUILT_SOURCES =

check_PROGRAMS = 
if BUILD_SNTP
check_PROGRAMS += tests
endif

base_SOURCES =				\
	$(srcdir)/../tests_main.cpp	\
	$(NULL)

tests_SOURCES =			\
	$(base_SOURCES)		\
	crypto.cpp		\
	keyFile.cpp		\
	kodDatabase.cpp		\
	kodFile.cpp		\
	networking.cpp		\
	packetHandling.cpp	\
	packetProcessing.cpp	\
	utilities.cpp		\
	$(NULL)

noinst_HEADERS =		\
	sntptest.h		\
	fileHandlingTest.h	\
	$(NULL)

dist_check_SCRIPTS =	tests-runner

EXTRA_DIST =				\
	data/debug-input-lfp-bin	\
	data/debug-input-lfp-dec	\
	data/debug-input-pkt		\
	data/key-test-ascii		\
	data/key-test-comments		\
	data/key-test-empty		\
	data/key-test-hex		\
	data/key-test-invalid-hex	\
	data/kod-expected-multiple	\
	data/kod-expected-single	\
	data/kod-test-blanks		\
	data/kod-test-correct		\
	data/kod-test-empty		\
	$(NULL)

CLEANFILES =				\
	data/kod-output-multiple	\
	data/kod-output-single		\
	data/debug-output-pkt		\
	data/debug-output-lfp-dec	\
	data/kod-output-blank		\
	data/debug-output-lfp-bin	\
	$(NULL)

LDADD =						\
	../libsntp.a				\
	$(LIBOPTS_LDADD)			\
	$(LDADD_LIBEVENT)			\
	$(top_builddir)/../libntp/libntp.a	\
	$(LDADD_LIBNTP)				\
	$(LDADD_NTP)				\
	$(GTEST_LDFLAGS)			\
	$(GTEST_LIBS)				\
	$(NULL)

AM_CFLAGS   = $(CFLAGS_NTP)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)

AM_CPPFLAGS  = $(SNTP_INCS)
AM_CPPFLAGS += -I$(srcdir)/..
AM_CPPFLAGS += -I$(top_srcdir)/../tests
AM_CPPFLAGS += $(LIBOPTS_CFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_LIBEVENT)
AM_CPPFLAGS += $(GTEST_CPPFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)

AM_LDFLAGS = $(LDFLAGS_NTP)

TESTS = 

if !NTP_CROSSCOMPILE
TESTS += tests-runner
endif

## ensure ../libsntp.a is built

BUILT_SOURCES	+= check-libsntp
CLEANFILES	+= check-libsntp

check-libsntp: ../libsntp.a
	@echo stamp > $@

../libsntp.a:
	cd .. && $(MAKE) $(AM_MAKEFLAGS) libsntp.a

## check-libntp.mf - automake fragment
## slightly adapted for deeper directory

BUILT_SOURCES	+= check-libntp
CLEANFILES	+= check-libntp

check-libntp: ../../libntp/libntp.a
	@echo stamp > $@

../../libntp/libntp.a:
	cd ../../libntp && $(MAKE) $(AM_MAKEFLAGS) libntp.a

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
