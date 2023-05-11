#AUTOMAKE_OPTIONS = foreign 1.9 subdir-objects
NULL =
BUILT_SOURCES =
CLEANFILES =
DISTCLEANFILES =

std_unity_list =	 						\
	$(abs_top_srcdir)/sntp/unity/auto/generate_test_runner.rb	\
	$(abs_srcdir)/testconf.yml					\
	$(NULL)

run_unity =	ruby $(std_unity_list)

EXTRA_DIST =				\
	testconf.yml			\
	$(NULL)

check_PROGRAMS = sec-2853

# HMS: we may not need some of these:
LDADD =					\
	$(top_builddir)/sntp/unity/libunity.a	\
	$(top_builddir)/libntp/libntp.a	\
	$(top_builddir)/ntpd/libntpd.a	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(LIBM)				\
	$(NULL)

AM_CFLAGS   = $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

# HMS: we may not need some of these:
AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS  = $(LDFLAGS_NTP)
AM_LDFLAGS += $(NTP_HARD_LDFLAGS)

sec_2853_SOURCES = 			\
	run-sec-2853.c			\
	sec-2853.c			\
	$(NULL)

BUILT_SOURCES += $(srcdir)/run-sec-2853.c

$(srcdir)/run-sec-2853.c: $(srcdir)/sec-2853.c $(std_unity_list)
	$(run_unity) $< $@

# HMS: we may not need some of these:
#noinst_HEADERS =	ntpdtest.h	\
#			$(NULL)

TESTS =

if !NTP_CROSSCOMPILE
TESTS += $(check_PROGRAMS)
endif

include $(top_srcdir)/check-libntp.mf
include $(top_srcdir)/check-libntpd.mf
include $(top_srcdir)/check-libunity.mf
include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
