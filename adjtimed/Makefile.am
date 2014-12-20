## adjtimed Makefile.am

bin_PROGRAMS = $(ADJTIMED_DB)
libexec_PROGRAMS = $(ADJTIMED_DL)
sbin_PROGRAMS =	$(ADJTIMED_DS)

BUILT_SOURCES =
CLEANFILES =
EXTRA_PROGRAMS = adjtimed

AM_CFLAGS = $(CFLAGS_NTP)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)

LDADD = ../libntp/libntp.a $(LDADD_LIBNTP) $(LIBM) $(PTHREAD_LIBS)

include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/sntp/check-libntp.mf
include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
