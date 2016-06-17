NULL =
BUILT_SOURCES =
CLEANFILES =

std_unity_list = 						\
	$(top_srcdir)/sntp/unity/auto/generate_test_runner.rb	\
	$(NULL)

run_unity =	cd $(srcdir) && ruby ../../sntp/unity/auto/generate_test_runner.rb

check_PROGRAMS = 	\
	test-ntpq	\
	$(NULL)

LDADD =					\
	$(top_builddir)/ntpq/libntpq.a	\
	$(top_builddir)/ntpd/libntpd.a	\
	$(top_builddir)/libntp/libntp.a	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(NULL)

unity_tests_LDADD =				\
	$(LDADD)				\
	$(top_builddir)/sntp/unity/libunity.a	\
	$(LIBM)					\
	$(NULL)

AM_CFLAGS   = $(CFLAGS_NTP)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/ntpq
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += $(LIBOPTS_CFLAGS)

AM_LDFLAGS = $(LDFLAGS_NTP)

BUILT_SOURCES +=			\
	$(srcdir)/run-t-ntpq.c		\
	$(NULL)

noinst_HEADERS =			\
	$(NULL)

###

test_ntpq_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_ntpq_SOURCES =			\
	t-ntpq.c			\
	run-t-ntpq.c			\
	$(NULL)

$(srcdir)/run-t-ntpq.c: $(srcdir)/t-ntpq.c $(std_unity_list)
	$(run_unity) t-ntpq.c run-t-ntpq.c


TESTS =

if !NTP_CROSSCOMPILE
TESTS += $(check_PROGRAMS)
endif

## check-libntp.mf - automake fragment
## slightly adapted for deeper directory

BUILT_SOURCES	+= check-libntpd check-libntp check-libunity 
CLEANFILES	+= check-libntpd check-libntp check-libunity 

check-libntpd: ../../ntpd/libntpd.a
	@echo stamp > $@

../../ntpd/libntpd.a:
	cd ../../ntpd && $(MAKE) $(AM_MAKEFLAGS) libntpd.a


check-libntp: ../../libntp/libntp.a
	@echo stamp > $@

../../libntp/libntp.a:
	cd ../../libntp && $(MAKE) $(AM_MAKEFLAGS) libntp.a


check-libunity: ../../sntp/unity/libunity.a
	@echo stamp > $@

../../sntp/unity/libunity.a:
	cd ../../sntp/unity && $(MAKE) $(AM_MAKEFLAGS) libunity.a


include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
