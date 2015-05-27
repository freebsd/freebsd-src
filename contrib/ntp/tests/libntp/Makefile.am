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
AM_CPPFLAGS += $(GTEST_CPPFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)

AM_LDFLAGS = $(LDFLAGS_NTP)

tests_SOURCES = $(top_srcdir)/sntp/tests_main.cpp	\
		libntptest.cpp		\
		a_md5encrypt.cpp	\
		atoint.cpp		\
		atouint.cpp		\
		authkeys.cpp		\
		buftvtots.cpp		\
		calendar.cpp		\
		caljulian.cpp		\
		caltontp.cpp		\
		calyearstart.cpp	\
		clocktime.cpp		\
		decodenetnum.cpp	\
		hextoint.cpp		\
		hextolfp.cpp		\
		humandate.cpp		\
		lfpfunc.cpp		\
		lfptostr.cpp		\
		modetoa.cpp		\
		msyslog.cpp		\
		netof.cpp		\
		numtoa.cpp		\
		numtohost.cpp		\
		octtoint.cpp		\
		prettydate.cpp		\
		recvbuff.cpp		\
		refnumtoa.cpp		\
		sfptostr.cpp		\
		socktoa.cpp		\
		ssl_init.cpp		\
		statestr.cpp		\
		strtolfp.cpp		\
		timespecops.cpp		\
		timestructs.cpp		\
		timevalops.cpp		\
		tstotv.cpp		\
		tvtots.cpp		\
		uglydate.cpp		\
		vi64ops.cpp		\
		ymd2yd.cpp		\
		$(NULL)

noinst_HEADERS =	lfptest.h	\
			libntptest.h	\
			sockaddrtest.h	\
			timestructs.h	\
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
