NULL =
BUILT_SOURCES =
EXTRA_PROGRAMS =

# data CLEANFILES down below

CLEANFILES =			\
	debug-output-lfp-bin	\
	debug-output-lfp-dec	\
	debug-output-pkt	\
	kod-output-blank	\
	kod-output-multiple	\
	kod-output-single	\
	testLogfile.log		\
	testLogfile2.log	\
	.version		\
	version.c		\
	$(NULL)

DISTCLEANFILES =		\
	kod-output-blank	\
	kod-output-single 	\
	kod-output-multiple	\
	testLogfile.log		\
	testLogfile2.log	\
	$(NULL)

std_unity_list =						\
	$(abs_top_srcdir)/unity/auto/generate_test_runner.rb	\
	$(abs_srcdir)/testconf.yml				\
	$(NULL)

run_unity =	ruby $(std_unity_list)

# Use EXTRA_PROGRAMS for test files that are under development but
# not production-ready
#EXTRA_PROGRAMS +=		test-packetHandling test-packetProcessing

check_PROGRAMS =			\
	test-crypto			\
	test-keyFile			\
	test-log			\
	test-networking			\
	test-packetHandling		\
	test-packetProcessing		\
	test-utilities			\
	$(NULL)

if BUILD_TEST_KODDATABASE
check_PROGRAMS += test-kodDatabase
endif

if BUILD_TEST_KODFILE
check_PROGRAMS += test-kodFile
endif

noinst_HEADERS =		\
	sntptest.h		\
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
	testconf.yml			\
	$(NULL)

CLEANFILES +=				\
	data/kod-output-multiple	\
	data/kod-output-single		\
	data/debug-output-pkt		\
	data/debug-output-lfp-dec	\
	data/kod-output-blank		\
	data/debug-output-lfp-bin	\
	$(NULL)

#split into LDADD and tests_LDADD?
base_LDADD =					\
	../libsntp.a				\
	$(LIBOPTS_LDADD)			\
	$(LDADD_LIBEVENT)			\
	$(top_builddir)/../libntp/libntp.a	\
	$(LDADD_LIBNTP)				\
	$(LDADD_NTP)				\
	$(NULL)

LDADD =						\
	$(base_LDADD)				\
	$(top_builddir)/unity/libunity.a	\
	$(LIBM)					\
	$(NULL)

AM_CFLAGS  = $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

AM_CPPFLAGS  = $(SNTP_INCS)
AM_CPPFLAGS += -I$(srcdir)/..
AM_CPPFLAGS += -I$(top_srcdir)/../tests
AM_CPPFLAGS += $(LIBOPTS_CFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_LIBEVENT)
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/unity
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS  = $(LDFLAGS_NTP)
AM_LDFLAGS += $(NTP_HARD_LDFLAGS)

BUILT_SOURCES +=				\
	$(srcdir)/run-crypto.c			\
	$(srcdir)/run-keyFile.c			\
	$(srcdir)/run-kodDatabase.c		\
	$(srcdir)/run-kodFile.c			\
	$(srcdir)/run-networking.c		\
	$(srcdir)/run-packetHandling.c		\
	$(srcdir)/run-packetProcessing.c	\
	$(srcdir)/run-utilities.c		\
	$(NULL)

test_networking_SOURCES =		\
	networking.c			\
	run-networking.c		\
	$(NULL)

nodist_test_networking_SOURCES =	version.c

test_packetHandling_SOURCES =		\
	packetHandling.c		\
	run-packetHandling.c		\
	$(NULL)

nodist_test_packetHandling_SOURCES =	version.c

test_kodDatabase_SOURCES =		\
	kodDatabase.c			\
	run-kodDatabase.c		\
	$(NULL)

nodist_test_kodDatabase_SOURCES =	version.c

test_keyFile_SOURCES =			\
	fileHandlingTest.c		\
	keyFile.c			\
	run-keyFile.c			\
	$(NULL)

nodist_test_keyFile_SOURCES =		version.c

test_kodFile_SOURCES =			\
	fileHandlingTest.c		\
	kodFile.c			\
	run-kodFile.c			\
	$(NULL)

nodist_test_kodFile_SOURCES =		version.c

test_packetProcessing_SOURCES =		\
	packetProcessing.c		\
	run-packetProcessing.c		\
	sntptest.c			\
	$(NULL)

nodist_test_packetProcessing_SOURCES =	version.c

test_utilities_SOURCES =		\
	fileHandlingTest.c		\
	utilities.c			\
	run-utilities.c			\
	$(NULL)

nodist_test_utilities_SOURCES =		version.c

test_crypto_CFLAGS =			\
	$(CFLAGS_LIBEVENT)		\
	$(AM_CFLAGS)			\
	$(NULL)

test_crypto_SOURCES =			\
	crypto.c			\
	run-crypto.c			\
	$(NULL)

nodist_test_crypto_SOURCES =		version.c

test_log_SOURCES =			\
	t-log.c				\
	run-t-log.c			\
	$(NULL)

nodist_test_log_SOURCES =		version.c


$(srcdir)/run-kodFile.c: $(srcdir)/kodFile.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-keyFile.c: $(srcdir)/keyFile.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-kodDatabase.c: $(srcdir)/kodDatabase.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-networking.c: $(srcdir)/networking.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-packetProcessing.c: $(srcdir)/packetProcessing.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-packetHandling.c: $(srcdir)/packetHandling.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-utilities.c: $(srcdir)/utilities.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-crypto.c: $(srcdir)/crypto.c $(std_unity_list)
	$(run_unity) $< $@

$(srcdir)/run-t-log.c: $(srcdir)/t-log.c $(std_unity_list)
	$(run_unity) $< $@

TESTS = 

if !NTP_CROSSCOMPILE
TESTS += $(check_PROGRAMS)
endif
#if !NTP_CROSSCOMPILE
#TESTS += tests-runner
#endif

version.c: Makefile $(top_srcdir)/scm-rev
	$(AM_V_GEN)env CSET=`cat $(srcdir)/../scm-rev` $(top_builddir)/../scripts/build/mkver sntp-tests

$(top_srcdir)/scm-rev: FRC.scm-rev
	$(AM_V_GEN)cd .. && $(MAKE) $(AM_MAKEFLAGS) check-scm-rev

FRC.scm-rev:
	@: FRC.scm-rev "force" depends on nothing and is not a file, so is	\
	   always out-of-date causing targets which depend on it to also	\
	   be outdated so their rules to fire each time they are built.

include $(top_srcdir)/check-libntp.mf
include $(top_srcdir)/check-libsntp.mf
include $(top_srcdir)/check-libunity.mf
include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
