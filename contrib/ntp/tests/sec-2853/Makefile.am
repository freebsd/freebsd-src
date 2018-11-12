#AUTOMAKE_OPTIONS = foreign 1.9 subdir-objects
NULL =
BUILT_SOURCES =
CLEANFILES =

std_unity_list = 						\
	$(top_srcdir)/sntp/unity/auto/generate_test_runner.rb	\
	$(NULL)

run_unity =	cd $(srcdir) && ruby ../../sntp/unity/auto/generate_test_runner.rb

check_PROGRAMS = sec-2853

# HMS: we may not need some of these:
LDADD =					\
	$(top_builddir)/sntp/unity/libunity.a	\
	$(top_builddir)/ntpd/rc_cmdlength.o	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(NULL)

AM_CFLAGS   = $(CFLAGS_NTP)

# HMS: we may not need some of these:
AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity

AM_LDFLAGS = $(LDFLAGS_NTP)

sec_2853_SOURCES = 			\
	run-sec-2853.c			\
	sec-2853.c			\
	$(NULL)

BUILT_SOURCES += $(srcdir)/run-sec-2853.c

$(srcdir)/run-sec-2853.c: $(srcdir)/sec-2853.c $(std_unity_list)
	$(run_unity) sec-2853.c run-sec-2853.c

# HMS: we may not need some of these:
#noinst_HEADERS =	ntpdtest.h	\
#			$(NULL)

TESTS =

if !NTP_CROSSCOMPILE
TESTS += $(check_PROGRAMS)
endif

## check-libntp.mf - automake fragment
## slightly adapted for deeper directory

BUILT_SOURCES	+= check-libntp check-libunity check-rc-cmdlength
CLEANFILES	+= check-libntp check-libunity check-rc-cmdlength

check-libntp: ../../libntp/libntp.a
	@echo stamp > $@

../../libntp/libntp.a:
	cd ../../libntp && $(MAKE) $(AM_MAKEFLAGS) libntp.a

check-libunity: ../../sntp/unity/libunity.a
	@echo stamp > $@

../../sntp/unity/libunity.a:
	cd ../../libunity && $(MAKE) $(AM_MAKEFLAGS) libunity.a

check-rc-cmdlength: ../../ntpd/rc_cmdlength.o
	@echo stamp > $@

../../ntpd/rc_cmdlength.o:
	cd ../../ntpd && $(MAKE) $(AM_MAKEFLAGS) rc_cmdlength.o

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
