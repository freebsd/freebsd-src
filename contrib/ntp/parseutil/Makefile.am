NULL=

BUILT_SOURCES =
CLEANFILES =
noinst_PROGRAMS = @TESTDCF@ @DCFD@
EXTRA_PROGRAMS = testdcf dcfd

AM_CFLAGS = $(CFLAGS_NTP)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)

LDADD = ../libntp/libntp.a $(LDADD_LIBNTP) $(LIBM)

ETAGS_ARGS = Makefile.am
DISTCLEANFILES = $(EXTRA_PROGRAMS)

check-local: @DCFD@
	case "$(noinst_PROGRAMS)" in	\
	 *dcfd*) ./dcfd -Y ;;		\
	esac

include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
