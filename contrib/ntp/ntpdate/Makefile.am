NULL =

BUILT_SOURCES =
DISTCLEANFILES =

bin_PROGRAMS = $(NTPDATE_DB)
libexec_PROGRAMS = $(NTPDATE_DL)
sbin_PROGRAMS = $(NTPDATE_DS)

EXTRA_PROGRAMS = ntpdate

AM_CFLAGS = $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS = $(LDFLAGS_NTP)
AM_LDFLAGS += $(NTP_HARD_LDFLAGS)

# LDADD might need RESLIB and ADJLIB
LDADD = ../libntp/libntp.a
ntpdate_LDADD  = $(LDADD) $(LDADD_LIBNTP) $(PTHREAD_LIBS) $(LIBOPTS_LDADD)
ntpdate_LDADD += $(LIBM) $(LDADD_NTP)
CLEANFILES = .version version.c
noinst_HEADERS=	ntpdate.h

ntpdate_SOURCES = ntpdate.c
nodist_ntpdate_SOURCES = version.c

version.c: Makefile $(top_srcdir)/sntp/scm-rev
	$(AM_V_GEN)env CSET=`cat $(top_srcdir)/sntp/scm-rev` $(top_builddir)/scripts/build/mkver ntpdate
	
include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/check-libntp.mf
include $(top_srcdir)/check-scm-rev.mf
include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
