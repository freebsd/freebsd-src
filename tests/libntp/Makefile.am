NULL =
BUILT_SOURCES =
CLEANFILES =

std_unity_list =	 						\
	$(abs_top_srcdir)/sntp/unity/auto/generate_test_runner.rb	\
	$(abs_srcdir)/testconf.yml					\
	$(NULL)

run_unity =	ruby $(std_unity_list)

EXTRA_DIST =				\
	testconf.yml			\
	$(NULL)

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
	test-tsafememcmp	\
	test-tstotv		\
	test-tvtots		\
	test-uglydate		\
	test-vi64ops		\
	test-ymd2yd		\
	$(NULL)

LDADD =					\
	$(top_builddir)/sntp/unity/libunity.a	\
	$(top_builddir)/libntp/libntp.a	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(LIBM)				\
	$(NULL)

AM_CFLAGS  = $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/sntp
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS  = $(LDFLAGS_NTP)
AM_LDFLAGS += $(NTP_HARD_LDFLAGS)

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
	$(srcdir)/run-tsafememcmp.c	\
	$(srcdir)/run-tstotv.c		\
	$(srcdir)/run-tvtots.c		\
	$(srcdir)/run-uglydate.c	\
	$(srcdir)/run-vi64ops.c		\
	$(srcdir)/run-ymd2yd.c		\
	$(NULL)

noinst_HEADERS =	\
	lfptest.h	\
	sockaddrtest.h	\
	test-libntp.h	\
	$(NULL)

###

test_a_md5encrypt_SOURCES =	\
	a_md5encrypt.c		\
	run-a_md5encrypt.c	\
	$(NULL)

$(srcdir)/run-a_md5encrypt.c: $(srcdir)/a_md5encrypt.c $(std_unity_list)
	$(run_unity) $< $@

###

test_atoint_SOURCES =		\
	atoint.c		\
	run-atoint.c		\
	$(NULL)

$(srcdir)/run-atoint.c: $(srcdir)/atoint.c $(std_unity_list)
	$(run_unity) $< $@

###

test_atouint_SOURCES =		\
	atouint.c		\
	run-atouint.c		\
	$(NULL)

$(srcdir)/run-atouint.c: $(srcdir)/atouint.c $(std_unity_list)
	$(run_unity) $< $@

###

test_authkeys_SOURCES =		\
	authkeys.c		\
	run-authkeys.c		\
	$(NULL)

$(srcdir)/run-authkeys.c: $(srcdir)/authkeys.c $(std_unity_list)
	$(run_unity) $< $@

###

test_buftvtots_SOURCES =	\
	buftvtots.c		\
	run-buftvtots.c		\
	lfptest.c		\
	$(NULL)

$(srcdir)/run-buftvtots.c: $(srcdir)/buftvtots.c $(std_unity_list)
	$(run_unity) $< $@

###

test_calendar_SOURCES =		\
	calendar.c		\
	run-calendar.c		\
	test-libntp.c		\
	$(NULL)

$(srcdir)/run-calendar.c: $(srcdir)/calendar.c $(std_unity_list)
	$(run_unity) $< $@

###

test_caltontp_CFLAGS =		\
	$(CFLAGS_LIBEVENT)	\
	$(AM_CFLAGS)		\
	$(NULL)

test_caltontp_SOURCES =		\
	caltontp.c		\
	run-caltontp.c		\
	$(NULL)

$(srcdir)/run-caltontp.c: $(srcdir)/caltontp.c $(std_unity_list)
	$(run_unity) $< $@

###

test_caljulian_CFLAGS =		\
	$(CFLAGS_LIBEVENT)	\
	$(AM_CFLAGS)		\
	$(NULL)

test_caljulian_SOURCES =	\
	caljulian.c		\
	run-caljulian.c		\
	test-libntp.c		\
	$(NULL)

$(srcdir)/run-caljulian.c: $(srcdir)/caljulian.c $(std_unity_list)
	$(run_unity) $< $@

###

test_calyearstart_SOURCES =	\
	calyearstart.c		\
	run-calyearstart.c	\
	test-libntp.c		\
	$(NULL)

$(srcdir)/run-calyearstart.c: $(srcdir)/calyearstart.c $(std_unity_list)
	$(run_unity) $< $@

###

test_clocktime_SOURCES =	\
	clocktime.c		\
	run-clocktime.c		\
	test-libntp.c		\
	$(NULL)

$(srcdir)/run-clocktime.c: $(srcdir)/clocktime.c $(std_unity_list)
	$(run_unity) $< $@

###

test_decodenetnum_CFLAGS =	\
	$(CFLAGS_LIBEVENT)	\
	$(AM_CFLAGS)		\
	$(NULL)

test_decodenetnum_SOURCES =	\
	decodenetnum.c		\
	run-decodenetnum.c	\
	sockaddrtest.c		\
	$(NULL)

$(srcdir)/run-decodenetnum.c: $(srcdir)/decodenetnum.c $(std_unity_list)
	$(run_unity) $< $@

###

test_hextoint_SOURCES =		\
	hextoint.c		\
	run-hextoint.c		\
	$(NULL)

$(srcdir)/run-hextoint.c: $(srcdir)/hextoint.c $(std_unity_list)
	$(run_unity) $< $@

###

test_hextolfp_SOURCES =		\
	hextolfp.c		\
	run-hextolfp.c		\
	lfptest.c		\
	$(NULL)

$(srcdir)/run-hextolfp.c: $(srcdir)/hextolfp.c $(std_unity_list)
	$(run_unity) $< $@

###

test_humandate_SOURCES =	\
	humandate.c		\
	run-humandate.c		\
	$(NULL)

$(srcdir)/run-humandate.c: $(srcdir)/humandate.c $(std_unity_list)
	$(run_unity) $< $@

###

test_lfpfunc_SOURCES =		\
	lfpfunc.c		\
	run-lfpfunc.c		\
	$(NULL)

$(srcdir)/run-lfpfunc.c: $(srcdir)/lfpfunc.c $(std_unity_list)
	$(run_unity) $< $@

###

test_lfptostr_SOURCES =		\
	lfptostr.c		\
	run-lfptostr.c		\
	$(NULL)

$(srcdir)/run-lfptostr.c: $(srcdir)/lfptostr.c $(std_unity_list)
	$(run_unity) $< $@

###

test_modetoa_SOURCES =		\
	modetoa.c		\
	run-modetoa.c		\
	$(NULL)

$(srcdir)/run-modetoa.c: $(srcdir)/modetoa.c $(std_unity_list)
	$(run_unity) $< $@

###

test_msyslog_SOURCES =		\
	msyslog.c		\
	run-msyslog.c		\
	$(NULL)

$(srcdir)/run-msyslog.c: $(srcdir)/msyslog.c $(std_unity_list)
	$(run_unity) $< $@

###

test_netof_SOURCES =		\
	netof.c			\
	run-netof.c		\
	sockaddrtest.c		\
	$(NULL)

$(srcdir)/run-netof.c: $(srcdir)/netof.c $(std_unity_list)
	$(run_unity) $< $@

###

test_numtoa_SOURCES =		\
	numtoa.c		\
	run-numtoa.c		\
	$(NULL)

$(srcdir)/run-numtoa.c: $(srcdir)/numtoa.c $(std_unity_list)
	$(run_unity) $< $@

###

test_numtohost_SOURCES =	\
	numtohost.c		\
	run-numtohost.c		\
	$(NULL)

$(srcdir)/run-numtohost.c: $(srcdir)/numtohost.c $(std_unity_list)
	$(run_unity) $< $@

###

test_octtoint_SOURCES =		\
	octtoint.c		\
	run-octtoint.c		\
	$(NULL)

$(srcdir)/run-octtoint.c: $(srcdir)/octtoint.c $(std_unity_list)
	$(run_unity) $< $@

###

test_prettydate_SOURCES =	\
	prettydate.c		\
	run-prettydate.c	\
	$(NULL)

$(srcdir)/run-prettydate.c: $(srcdir)/prettydate.c $(std_unity_list)
	$(run_unity) $< $@

###

test_recvbuff_SOURCES =		\
	recvbuff.c		\
	run-recvbuff.c		\
	$(NULL)

$(srcdir)/run-recvbuff.c: $(srcdir)/recvbuff.c $(std_unity_list)
	$(run_unity) $< $@

###

test_refidsmear_SOURCES =	\
	refidsmear.c		\
	run-refidsmear.c	\
	$(NULL)

$(srcdir)/run-refidsmear.c: $(srcdir)/refidsmear.c $(std_unity_list)
	$(run_unity) $< $@

###

test_refnumtoa_SOURCES =	\
	refnumtoa.c		\
	run-refnumtoa.c		\
	$(NULL)

$(srcdir)/run-refnumtoa.c: $(srcdir)/refnumtoa.c $(std_unity_list)
	$(run_unity) $< $@

###

test_sfptostr_SOURCES =		\
	sfptostr.c		\
	run-sfptostr.c		\
	$(NULL)

$(srcdir)/run-sfptostr.c: $(srcdir)/sfptostr.c $(std_unity_list)
	$(run_unity) $< $@

###

test_socktoa_SOURCES =		\
	socktoa.c		\
	run-socktoa.c		\
	sockaddrtest.c		\
	$(NULL)

$(srcdir)/run-socktoa.c: $(srcdir)/socktoa.c $(std_unity_list)
	$(run_unity) $< $@

###

test_ssl_init_SOURCES =		\
	ssl_init.c		\
	run-ssl_init.c		\
	$(NULL)

$(srcdir)/run-ssl_init.c: $(srcdir)/ssl_init.c $(std_unity_list)
	$(run_unity) $< $@

###

test_statestr_SOURCES =		\
	statestr.c		\
	run-statestr.c		\
	$(NULL)

$(srcdir)/run-statestr.c: $(srcdir)/statestr.c $(std_unity_list)
	$(run_unity) $< $@

###

test_strtolfp_CFLAGS =		\
	$(CFLAGS_LIBEVENT)	\
	$(AM_CFLAGS)		\
	$(NULL)

test_strtolfp_SOURCES =		\
	strtolfp.c		\
	run-strtolfp.c		\
	lfptest.c		\
	$(NULL)

$(srcdir)/run-strtolfp.c: $(srcdir)/strtolfp.c $(std_unity_list)
	$(run_unity) $< $@

###

test_timespecops_SOURCES =	\
	timespecops.c		\
	run-timespecops.c	\
	$(NULL)

$(srcdir)/run-timespecops.c: $(srcdir)/timespecops.c $(std_unity_list)
	$(run_unity) $< $@

###

test_timevalops_SOURCES =	\
	timevalops.c		\
	run-timevalops.c	\
	$(NULL)

$(srcdir)/run-timevalops.c: $(srcdir)/timevalops.c $(std_unity_list)
	$(run_unity) $< $@

###

test_tsafememcmp_SOURCES =	\
	tsafememcmp.c		\
	run-tsafememcmp.c	\
	$(NULL)

$(srcdir)/run-tsafememcmp.c: $(srcdir)/tsafememcmp.c $(std_unity_list)
	$(run_unity) $< $@

###

test_tstotv_SOURCES =		\
	tstotv.c		\
	run-tstotv.c		\
	$(NULL)

$(srcdir)/run-tstotv.c: $(srcdir)/tstotv.c $(std_unity_list)
	$(run_unity) $< $@

###

test_tvtots_SOURCES =		\
	tvtots.c		\
	run-tvtots.c		\
	lfptest.c		\
	$(NULL)

$(srcdir)/run-tvtots.c: $(srcdir)/tvtots.c $(std_unity_list)
	$(run_unity) $< $@

###

test_uglydate_SOURCES =		\
	uglydate.c		\
	run-uglydate.c		\
	$(NULL)

$(srcdir)/run-uglydate.c: $(srcdir)/uglydate.c $(std_unity_list)
	$(run_unity) $< $@

###

test_vi64ops_SOURCES =		\
	vi64ops.c		\
	run-vi64ops.c		\
	$(NULL)

$(srcdir)/run-vi64ops.c: $(srcdir)/vi64ops.c $(std_unity_list)
	$(run_unity) $< $@

###

test_ymd2yd_SOURCES =		\
	ymd2yd.c		\
	run-ymd2yd.c		\
	$(NULL)

$(srcdir)/run-ymd2yd.c: $(srcdir)/ymd2yd.c $(std_unity_list)
	$(run_unity) $< $@

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
