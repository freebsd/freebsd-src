NULL =
BUILT_SOURCES =
CLEANFILES =

std_unity_list =						\
	$(top_srcdir)/sntp/unity/auto/generate_test_runner.rb	\
	$(NULL)

run_unity =	cd $(srcdir) && ruby ../../sntp/unity/auto/generate_test_runner.rb

check_PROGRAMS =		\
	test-leapsec		\
	test-ntp_prio_q		\
	$(NULL)
if BUILD_TEST_NTP_RESTRICT
check_PROGRAMS += test-ntp_restrict
endif
if BUILD_TEST_NTP_SCANNER
check_PROGRAMS += test-ntp_scanner
endif
if BUILD_TEST_NTP_SIGND
check_PROGRAMS += test-ntp_signd
endif
check_PROGRAMS += 		\
	test-rc_cmdlength	\
	$(NULL)

EXTRA_PROGRAMS =		\
	test-ntp_restrict	\
	test-ntp_scanner	\
	test-ntp_signd		\
	$(NULL)


LDADD =					\
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
AM_CPPFLAGS += -I$(top_srcdir)/sntp
AM_CPPFLAGS += -I$(top_srcdir)/ntpd
AM_CPPFLAGS += -I$(top_srcdir)/tests/libntp
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity

AM_LDFLAGS = $(LDFLAGS_NTP)

BUILT_SOURCES +=			\
	$(srcdir)/run-leapsec.c		\
	$(srcdir)/run-ntp_prio_q.c	\
	$(srcdir)/run-ntp_restrict.c	\
	$(srcdir)/run-rc_cmdlength.c	\
	$(srcdir)/run-t-ntp_signd.c	\
	$(NULL)

###

test_leapsec_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

# Might need pthread support
test_leapsec_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_leapsec_SOURCES =			\
	leapsec.c			\
	run-leapsec.c			\
	$(srcdir)/../libntp/test-libntp.c		\
	$(NULL)

$(srcdir)/run-leapsec.c: $(srcdir)/leapsec.c $(std_unity_list)
	$(run_unity) leapsec.c run-leapsec.c

###
test_ntp_prio_q_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_ntp_prio_q_LDADD =			\
	$(unity_tests_LDADD)		\
	../../ntpd/ntpdsim-ntp_prio_q.o	\
	$(NULL)

test_ntp_prio_q_SOURCES =			\
	ntp_prio_q.c				\
	run-ntp_prio_q.c			\
	$(srcdir)/../libntp/test-libntp.c	\
	$(NULL)

$(srcdir)/run-ntp_prio_q.c: $(srcdir)/ntp_prio_q.c $(std_unity_list)
	$(run_unity) ntp_prio_q.c run-ntp_prio_q.c


###
test_ntp_restrict_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_ntp_restrict_LDADD =		\
	$(unity_tests_LDADD)		\
	$(NULL)

test_ntp_restrict_SOURCES =		\
	ntp_restrict.c			\
	run-ntp_restrict.c		\
	$(srcdir)/../libntp/test-libntp.c	\
	$(NULL)

$(srcdir)/run-ntp_restrict.c: $(srcdir)/ntp_restrict.c $(std_unity_list)
	$(run_unity) ntp_restrict.c run-ntp_restrict.c



###
test_rc_cmdlength_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_rc_cmdlength_LDADD =		\
	$(unity_tests_LDADD)		\
	$(NULL)

test_rc_cmdlength_SOURCES =		\
	rc_cmdlength.c			\
	run-rc_cmdlength.c		\
	$(srcdir)/../libntp/test-libntp.c	\
	$(NULL)

$(srcdir)/run-rc_cmdlength.c: $(srcdir)/rc_cmdlength.c $(std_unity_list)
	$(run_unity) rc_cmdlength.c run-rc_cmdlength.c

###

test_ntp_signd_LDADD =			\
	$(unity_tests_LDADD)		\
	$(top_builddir)/ntpd/ntp_config.o	\
	$(top_builddir)/ntpd/ntp_io.o	\
	$(NULL)

test_ntp_signd_SOURCES =			\
	t-ntp_signd.c				\
	run-t-ntp_signd.c			\
	$(srcdir)/../libntp/test-libntp.c	\
	$(srcdir)/../../ntpd/ntp_signd.c	\
	$(NULL)

$(srcdir)/run-t-ntp_signd.c: $(srcdir)/t-ntp_signd.c $(std_unity_list)
	$(run_unity) t-ntp_signd.c run-t-ntp_signd.c

###
test_ntp_scanner_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_ntp_scanner_LDADD =		\
	$(unity_tests_LDADD)		\
	$(top_builddir)/ntpd/ntp_config.o	\
	$(top_builddir)/ntpd/ntp_parser.o	\
	$(NULL)

test_ntp_scanner_SOURCES =			\
	t-ntp_scanner.c				\
	run-t-ntp_scanner.c			\
	$(srcdir)/../libntp/test-libntp.c	\
	$(NULL)

$(srcdir)/run-t-ntp_scanner.c: $(srcdir)/t-ntp_scanner.c $(std_unity_list)
	$(run_unity) t-ntp_scanner.c run-t-ntp_scanner.c


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

$(top_builddir)/ntpd/ntpdsim-ntp_prio_q.o:
	cd ../../ntpd/ && $(MAKE) $(AM_MAKEFLAGS) ntpdsim-ntp_prio_q.o

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
