#AUTOMAKE_OPTIONS = foreign 2.9 subdir-objects

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

check_PROGRAMS =	\
	first-test	\
	smeartest	\
	$(NULL)

EXTRA_PROGRAMS =	\
	bug-2803	\
	second-test	\
	$(NULL)

# HMS: we may not need some of these:
LDADD =					\
	$(top_builddir)/sntp/unity/libunity.a	\
	$(top_builddir)/libntp/libntp.a	\
	$(LDADD_LIBNTP)			\
	$(PTHREAD_LIBS)			\
	$(LDADD_NTP)			\
	$(LIBM)				\
	$(NULL)

AM_CFLAGS	= $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

# HMS: we may not need some of these:
AM_CPPFLAGS	= $(NTP_INCS)
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += -DUNITY_INCLUDE_CONFIG_H
AM_CPPFLAGS += -I$(top_srcdir)/sntp/unity
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS  = $(LDFLAGS_NTP)
AM_LDFLAGS += $(NTP_HARD_LDFLAGS)

bug_2803_SOURCES =		\
	bug-2803.c		\
	run-ut-2803.c		\
	ut-2803.c		\
	$(NULL)

$(srcdir)/run-ut-2803.c: $(srcdir)/ut-2803.c $(std_unity_list)
	$(run_unity) $< $@
BUILT_SOURCES	+= run-ut-2803.c

first_test_SOURCES =		\
	uglydate.c		\
	run-uglydate.c		\
	$(NULL)

$(srcdir)/run-uglydate.c: $(srcdir)/uglydate.c $(std_unity_list)
	$(run_unity) $< $@
BUILT_SOURCES	+= run-uglydate.c

second_test_SOURCES =		\
	modetoa.c		\
	run-modetoa.c		\
	$(NULL)

$(srcdir)/run-modetoa.c: $(srcdir)/modetoa.c $(std_unity_list)
	$(run_unity) $< $@
BUILT_SOURCES	+= run-modetoa.c


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
	cd ../../libunity && $(MAKE) $(AM_MAKEFLAGS) libunity.a

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
