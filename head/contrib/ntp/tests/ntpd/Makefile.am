NULL =
BUILT_SOURCES =
CLEANFILES =

check_PROGRAMS = tests

LDADD =					\
	$(top_builddir)/libntp/libntp.a	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(GTEST_LDFLAGS)		\
	$(GTEST_LIBS)			\
	$(NULL)

AM_CFLAGS   = $(CFLAGS_NTP)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/sntp
AM_CPPFLAGS += -I$(top_srcdir)/ntpd
AM_CPPFLAGS += $(GTEST_CPPFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)

AM_LDFLAGS = $(LDFLAGS_NTP)

tests_SOURCES = $(top_srcdir)/sntp/tests_main.cpp	\
		ntpdtest.cpp		\
		$(top_srcdir)/ntpd/ntp_leapsec.c	\
		leapsec.cpp	\
		$(NULL)

noinst_HEADERS =	ntpdtest.h	\
			$(NULL)

TESTS =

if !NTP_CROSSCOMPILE
TESTS += tests
endif

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
