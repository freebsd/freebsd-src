NULL =
BUILT_SOURCES =
CLEANFILES =

run_unity =	cd $(srcdir) && ruby ../../sntp/unity/auto/generate_test_runner.rb

#removed test-libntp
check_PROGRAMS =		\
	test-a_md5encrypt	\
	test-atoint		\
	test-atouint		\
	test-authkeys		\
	test-buftvtots		\
	test-calendar		\
	test-caljulian		\
	test-caltontp		\
	test-calyearstart	\
	test-clocktime		\
	test-decodenetnum	\
	test-hextoint		\
	test-hextolfp		\
	test-humandate		\
	test-lfpfunc		\
	test-lfptostr		\
	test-modetoa		\
	test-msyslog		\
	test-netof		\
	test-numtoa		\
	test-numtohost		\
	test-octtoint		\
	test-prettydate		\
	test-recvbuff		\
	test-refidsmear		\
	test-refnumtoa		\
	test-sfptostr		\
	test-socktoa		\
	test-ssl_init		\
	test-statestr		\
	test-strtolfp		\
	test-timespecops	\
	test-timevalops		\
	test-tstotv		\
	test-tvtots		\
	test-uglydate		\
	test-vi64ops		\
	test-ymd2yd		\
	$(NULL)

if GTEST_AVAILABLE
check_PROGRAMS += tests
else
EXTRA_PROGRAMS = tests
endif

LDADD =					\
	$(top_builddir)/libntp/libntp.a	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(LIBM)				\
	$(NULL)

tests_LDADD =				\
	$(LDADD)			\
	$(GTEST_LDFLAGS)		\
	$(GTEST_LIBS)			\
	$(NULL)

unity_tests_LDADD =				\
	$(LDADD)				\
	$(top_builddir)/sntp/unity/libunity.a	\
	$(NULL)

AM_CFLAGS = $(CFLAGS_NTP)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/sntp
AM_CPPFLAGS += $(GTEST_CPPFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)

AM_LDFLAGS = $(LDFLAGS_NTP)

tests_SOURCES =					\
	$(top_srcdir)/sntp/tests_main.cpp	\
	g_libntptest.cpp	\
	g_a_md5encrypt.cpp	\
	g_atoint.cpp		\
	g_atouint.cpp		\
	g_authkeys.cpp		\
	g_buftvtots.cpp		\
	g_calendar.cpp		\
	g_caljulian.cpp		\
	g_caltontp.cpp		\
	g_calyearstart.cpp	\
	g_clocktime.cpp		\
	g_decodenetnum.cpp	\
	g_hextoint.cpp		\
	g_hextolfp.cpp		\
	g_humandate.cpp		\
	g_lfpfunc.cpp		\
	g_lfptostr.cpp		\
	g_modetoa.cpp		\
	g_msyslog.cpp		\
	g_netof.cpp		\
	g_numtoa.cpp		\
	g_numtohost.cpp		\
	g_octtoint.cpp		\
	g_prettydate.cpp	\
	g_recvbuff.cpp		\
	g_refnumtoa.cpp		\
	g_sfptostr.cpp		\
	g_socktoa.cpp		\
	g_ssl_init.cpp		\
	g_statestr.cpp		\
	g_strtolfp.cpp		\
	g_timespecops.cpp	\
	g_timestructs.cpp	\
	g_timevalops.cpp	\
	g_tstotv.cpp		\
	g_tvtots.cpp		\
	g_uglydate.cpp		\
	g_vi64ops.cpp		\
	g_ymd2yd.cpp		\
	$(NULL)

BUILT_SOURCES +=			\
	$(srcdir)/run-a_md5encrypt.c	\
	$(srcdir)/run-atoint.c		\
	$(srcdir)/run-atouint.c		\
	$(srcdir)/run-authkeys.c	\
	$(srcdir)/run-buftvtots.c	\
	$(srcdir)/run-calendar.c	\
	$(srcdir)/run-caljulian.c	\
	$(srcdir)/run-caltontp.c	\
	$(srcdir)/run-calyearstart.c	\
	$(srcdir)/run-clocktime.c	\
	$(srcdir)/run-decodenetnum.c	\
	$(srcdir)/run-hextoint.c	\
	$(srcdir)/run-hextolfp.c	\
	$(srcdir)/run-humandate.c	\
	$(srcdir)/run-lfpfunc.c		\
	$(srcdir)/run-lfptostr.c	\
	$(srcdir)/run-modetoa.c		\
	$(srcdir)/run-msyslog.c		\
	$(srcdir)/run-netof.c		\
	$(srcdir)/run-numtoa.c		\
	$(srcdir)/run-numtohost.c	\
	$(srcdir)/run-octtoint.c	\
	$(srcdir)/run-prettydate.c	\
	$(srcdir)/run-recvbuff.c	\
	$(srcdir)/run-refidsmear.c	\
	$(srcdir)/run-refnumtoa.c	\
	$(srcdir)/run-sfptostr.c	\
	$(srcdir)/run-socktoa.c		\
	$(srcdir)/run-ssl_init.c	\
	$(srcdir)/run-statestr.c	\
	$(srcdir)/run-strtolfp.c	\
	$(srcdir)/run-timevalops.c	\
	$(srcdir)/run-timespecops.c	\
	$(srcdir)/run-tstotv.c		\
	$(srcdir)/run-tvtots.c		\
	$(srcdir)/run-uglydate.c	\
	$(srcdir)/run-vi64ops.c		\
	$(srcdir)/run-ymd2yd.c		\
	$(NULL)

noinst_HEADERS =	\
	g_lfptest.h	\
	lfptest.h	\
	g_libntptest.h	\
	g_sockaddrtest.h	\
	sockaddrtest.h	\
	g_timestructs.h	\
	test-libntp.h	\
	$(NULL)

###

test_a_md5encrypt_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_a_md5encrypt_LDADD =		\
	$(unity_tests_LDADD)		\
	$(NULL)

test_a_md5encrypt_SOURCES =		\
	a_md5encrypt.c			\
	run-a_md5encrypt.c		\
	$(NULL)

$(srcdir)/run-a_md5encrypt.c: $(srcdir)/a_md5encrypt.c $(std_unity_list)
	$(run_unity) a_md5encrypt.c run-a_md5encrypt.c

###

test_atoint_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_atoint_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_atoint_SOURCES =			\
	atoint.c			\
	run-atoint.c			\
	$(NULL)

$(srcdir)/run-atoint.c: $(srcdir)/atoint.c $(std_unity_list)
	$(run_unity) atoint.c run-atoint.c

###

test_atouint_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_atouint_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_atouint_SOURCES =			\
	atouint.c			\
	run-atouint.c			\
	$(NULL)

$(srcdir)/run-atouint.c: $(srcdir)/atouint.c $(std_unity_list)
	$(run_unity) atouint.c run-atouint.c

###

test_authkeys_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_authkeys_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_authkeys_SOURCES =			\
	authkeys.c			\
	run-authkeys.c			\
	$(NULL)

$(srcdir)/run-authkeys.c: $(srcdir)/authkeys.c $(std_unity_list)
	$(run_unity) authkeys.c run-authkeys.c

###

test_buftvtots_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_buftvtots_SOURCES =		\
	buftvtots.c			\
	run-buftvtots.c			\
	$(NULL)

test_buftvtots_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	-DUNITY_INCLUDE_DOUBLE		\
	$(NULL)

$(srcdir)/run-buftvtots.c: $(srcdir)/buftvtots.c $(std_unity_list)
	$(run_unity) buftvtots.c run-buftvtots.c

###

test_calendar_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_calendar_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_calendar_SOURCES =			\
	calendar.c			\
	run-calendar.c			\
	test-libntp.c			\
	$(NULL)

$(srcdir)/run-calendar.c: $(srcdir)/calendar.c $(std_unity_list)
	$(run_unity) calendar.c run-calendar.c

###

test_caltontp_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_caltontp_LDADD =			\
	$(unity_tests_LDADD)		\
	-lpthread			\
	$(NULL)

test_caltontp_SOURCES =			\
	caltontp.c			\
	run-caltontp.c			\
	$(NULL)

$(srcdir)/run-caltontp.c: $(srcdir)/caltontp.c $(std_unity_list)
	$(run_unity) caltontp.c run-caltontp.c

###

test_caljulian_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_caljulian_LDADD =			\
	$(unity_tests_LDADD)		\
	-lpthread			\
	$(NULL)

test_caljulian_SOURCES =		\
	caljulian.c			\
	run-caljulian.c			\
	test-libntp.c			\
	$(NULL)

$(srcdir)/run-caljulian.c: $(srcdir)/caljulian.c $(std_unity_list)
	$(run_unity) caljulian.c run-caljulian.c

###

test_calyearstart_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_calyearstart_LDADD =		\
	$(unity_tests_LDADD)		\
	$(NULL)

test_calyearstart_SOURCES =		\
	calyearstart.c			\
	run-calyearstart.c		\
	test-libntp.c			\
	$(NULL)

$(srcdir)/run-calyearstart.c: $(srcdir)/calyearstart.c $(std_unity_list)
	$(run_unity) calyearstart.c run-calyearstart.c

###

test_clocktime_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_clocktime_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_clocktime_SOURCES =		\
	clocktime.c			\
	run-clocktime.c			\
	test-libntp.c			\
	$(NULL)

$(srcdir)/run-clocktime.c: $(srcdir)/clocktime.c $(std_unity_list)
	$(run_unity) clocktime.c run-clocktime.c

###

test_decodenetnum_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_decodenetnum_LDADD =		\
	$(unity_tests_LDADD)		\
	-lpthread			\
	$(NULL)

test_decodenetnum_SOURCES =		\
	decodenetnum.c			\
	run-decodenetnum.c		\
	$(NULL)

$(srcdir)/run-decodenetnum.c: $(srcdir)/decodenetnum.c $(std_unity_list)
	$(run_unity) decodenetnum.c run-decodenetnum.c

###

test_hextoint_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_hextoint_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_hextoint_SOURCES =			\
	hextoint.c			\
	run-hextoint.c			\
	$(NULL)

$(srcdir)/run-hextoint.c: $(srcdir)/hextoint.c $(std_unity_list)
	$(run_unity) hextoint.c run-hextoint.c

###

test_hextolfp_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_hextolfp_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_hextolfp_SOURCES =			\
	hextolfp.c			\
	run-hextolfp.c			\
	$(NULL)

$(srcdir)/run-hextolfp.c: $(srcdir)/hextolfp.c $(std_unity_list)
	$(run_unity) hextolfp.c run-hextolfp.c

###

test_humandate_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_humandate_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_humandate_SOURCES =		\
	humandate.c			\
	run-humandate.c			\
	$(NULL)

$(srcdir)/run-humandate.c: $(srcdir)/humandate.c $(std_unity_list)
	$(run_unity) humandate.c run-humandate.c

###

test_lfpfunc_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	-DUNITY_INCLUDE_DOUBLE		\
	$(NULL)

test_lfpfunc_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_lfpfunc_SOURCES =			\
	lfpfunc.c			\
	run-lfpfunc.c			\
	$(NULL)

$(srcdir)/run-lfpfunc.c: $(srcdir)/lfpfunc.c $(std_unity_list)
	$(run_unity) lfpfunc.c run-lfpfunc.c

###

test_lfptostr_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_lfptostr_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_lfptostr_SOURCES =			\
	lfptostr.c			\
	run-lfptostr.c			\
	$(NULL)

$(srcdir)/run-lfptostr.c: $(srcdir)/lfptostr.c $(std_unity_list)
	$(run_unity) lfptostr.c run-lfptostr.c

###

test_modetoa_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_modetoa_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_modetoa_SOURCES =			\
	modetoa.c			\
	run-modetoa.c			\
	$(NULL)

$(srcdir)/run-modetoa.c: $(srcdir)/modetoa.c $(std_unity_list)
	$(run_unity) modetoa.c run-modetoa.c

###

test_msyslog_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_msyslog_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_msyslog_SOURCES =			\
	msyslog.c			\
	run-msyslog.c			\
	$(NULL)

$(srcdir)/run-msyslog.c: $(srcdir)/msyslog.c $(std_unity_list)
	$(run_unity) msyslog.c run-msyslog.c

###

test_netof_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_netof_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_netof_SOURCES =			\
	netof.c				\
	run-netof.c			\
	$(NULL)

$(srcdir)/run-netof.c: $(srcdir)/netof.c $(std_unity_list)
	$(run_unity) netof.c run-netof.c

###

test_numtoa_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_numtoa_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_numtoa_SOURCES =			\
	numtoa.c			\
	run-numtoa.c			\
	$(NULL)

$(srcdir)/run-numtoa.c: $(srcdir)/numtoa.c $(std_unity_list)
	$(run_unity) numtoa.c run-numtoa.c

###

test_numtohost_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_numtohost_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_numtohost_SOURCES =		\
	numtohost.c			\
	run-numtohost.c			\
	$(NULL)

$(srcdir)/run-numtohost.c: $(srcdir)/numtohost.c $(std_unity_list)
	$(run_unity) numtohost.c run-numtohost.c

###

test_octtoint_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_octtoint_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_octtoint_SOURCES =			\
	octtoint.c			\
	run-octtoint.c			\
	$(NULL)

$(srcdir)/run-octtoint.c: $(srcdir)/octtoint.c $(std_unity_list)
	$(run_unity) octtoint.c run-octtoint.c

###

test_prettydate_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_prettydate_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_prettydate_SOURCES =		\
	prettydate.c			\
	run-prettydate.c		\
	$(NULL)

$(srcdir)/run-prettydate.c: $(srcdir)/prettydate.c $(std_unity_list)
	$(run_unity) prettydate.c run-prettydate.c

###

test_recvbuff_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_recvbuff_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_recvbuff_SOURCES =			\
	recvbuff.c			\
	run-recvbuff.c			\
	$(NULL)

$(srcdir)/run-recvbuff.c: $(srcdir)/recvbuff.c $(std_unity_list)
	$(run_unity) recvbuff.c run-recvbuff.c

###

test_refidsmear_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_refidsmear_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_refidsmear_SOURCES =		\
	refidsmear.c			\
	run-refidsmear.c		\
	$(NULL)

$(srcdir)/run-refidsmear.c: $(srcdir)/refidsmear.c $(std_unity_list)
	$(run_unity) refidsmear.c run-refidsmear.c

###

test_refnumtoa_CFLAGS =				\
	-I$(top_srcdir)/sntp/unity		\
	$(NULL)

test_refnumtoa_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_refnumtoa_SOURCES =		\
	refnumtoa.c			\
	run-refnumtoa.c			\
	$(NULL)

$(srcdir)/run-refnumtoa.c: $(srcdir)/refnumtoa.c $(std_unity_list)
	$(run_unity) refnumtoa.c run-refnumtoa.c

###

test_sfptostr_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	-DUNITY_INCLUDE_DOUBLE		\
	$(NULL)

test_sfptostr_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_sfptostr_SOURCES =			\
	sfptostr.c			\
	run-sfptostr.c			\
	$(NULL)

$(srcdir)/run-sfptostr.c: $(srcdir)/sfptostr.c $(std_unity_list)
	$(run_unity) sfptostr.c run-sfptostr.c

###

test_socktoa_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_socktoa_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_socktoa_SOURCES =			\
	socktoa.c			\
	run-socktoa.c			\
	$(NULL)

$(srcdir)/run-socktoa.c: $(srcdir)/socktoa.c $(std_unity_list)
	$(run_unity) socktoa.c run-socktoa.c

###

test_ssl_init_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_ssl_init_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_ssl_init_SOURCES =			\
	ssl_init.c			\
	run-ssl_init.c			\
	$(NULL)

$(srcdir)/run-ssl_init.c: $(srcdir)/ssl_init.c $(std_unity_list)
	$(run_unity) ssl_init.c run-ssl_init.c

###

test_statestr_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_statestr_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_statestr_SOURCES =			\
	statestr.c			\
	run-statestr.c			\
	$(NULL)

$(srcdir)/run-statestr.c: $(srcdir)/statestr.c $(std_unity_list)
	$(run_unity) statestr.c run-statestr.c

###

test_strtolfp_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_strtolfp_LDADD =			\
	$(unity_tests_LDADD)		\
	-lpthread			\
	$(NULL)

test_strtolfp_SOURCES =			\
	strtolfp.c			\
	run-strtolfp.c			\
	$(NULL)

$(srcdir)/run-strtolfp.c: $(srcdir)/strtolfp.c $(std_unity_list)
	$(run_unity) strtolfp.c run-strtolfp.c

###

test_timespecops_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_timespecops_LDADD =		\
	$(unity_tests_LDADD)		\
	$(NULL)

test_timespecops_SOURCES =		\
	timespecops.c			\
	run-timespecops.c		\
	$(NULL)

$(srcdir)/run-timespecops.c: $(srcdir)/timespecops.c $(std_unity_list)
	$(run_unity) timespecops.c run-timespecops.c

###

test_timevalops_CFLAGS =		\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_timevalops_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_timevalops_SOURCES =		\
	timevalops.c			\
	run-timevalops.c		\
	$(NULL)

$(srcdir)/run-timevalops.c: $(srcdir)/timevalops.c $(std_unity_list)
	$(run_unity) timevalops.c run-timevalops.c

###

test_tstotv_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_tstotv_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_tstotv_SOURCES =			\
	tstotv.c			\
	run-tstotv.c			\
	$(NULL)

$(srcdir)/run-tstotv.c: $(srcdir)/tstotv.c $(std_unity_list)
	$(run_unity) tstotv.c run-tstotv.c

###

test_tvtots_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	-DUNITY_INCLUDE_DOUBLE		\
	$(NULL)

test_tvtots_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_tvtots_SOURCES =			\
	tvtots.c			\
	run-tvtots.c			\
	$(NULL)

$(srcdir)/run-tvtots.c: $(srcdir)/tvtots.c $(std_unity_list)
	$(run_unity) tvtots.c run-tvtots.c

###

test_uglydate_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_uglydate_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_uglydate_SOURCES =			\
	uglydate.c			\
	run-uglydate.c			\
	$(NULL)

$(srcdir)/run-uglydate.c: $(srcdir)/uglydate.c $(std_unity_list)
	$(run_unity) uglydate.c run-uglydate.c

###

test_vi64ops_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_vi64ops_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_vi64ops_SOURCES =			\
	vi64ops.c			\
	run-vi64ops.c			\
	$(NULL)

$(srcdir)/run-vi64ops.c: $(srcdir)/vi64ops.c $(std_unity_list)
	$(run_unity) vi64ops.c run-vi64ops.c

###

test_ymd2yd_CFLAGS =			\
	-I$(top_srcdir)/sntp/unity	\
	$(NULL)

test_ymd2yd_LDADD =			\
	$(unity_tests_LDADD)		\
	$(NULL)

test_ymd2yd_SOURCES =			\
	ymd2yd.c			\
	run-ymd2yd.c			\
	$(NULL)

$(srcdir)/run-ymd2yd.c: $(srcdir)/ymd2yd.c $(std_unity_list)
	$(run_unity) ymd2yd.c run-ymd2yd.c

###

TESTS =

if !NTP_CROSSCOMPILE
TESTS += $(check_PROGRAMS)
endif

## check-libntp.mf - automake fragment
## slightly adapted for deeper directory

BUILT_SOURCES	+= check-libntp check-libunity
CLEANFILES	+= check-libntp check-libunity

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
