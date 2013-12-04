NULL=
#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS=	

if NTP_BINSUBDIR_IS_BIN
bin_PROGRAMS=	@MAKE_NTPTIME@ @MAKE_TICKADJ@ @MAKE_TIMETRIM@ \
		ntp-keygen
else
sbin_PROGRAMS=	@MAKE_NTPTIME@ @MAKE_TICKADJ@ @MAKE_TIMETRIM@ \
		ntp-keygen
endif

EXTRA_PROGRAMS=	audio-pcm byteorder hist jitter kern longsize \
	ntptime pps-api precision sht testrs6000 tg tickadj timetrim

AM_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/lib/isc/include \
	-I$(top_srcdir)/lib/isc/nothreads/include \
	-I$(top_srcdir)/lib/isc/unix/include $(LIBOPTS_CFLAGS)

# LDADD might need RESLIB and ADJLIB
LDADD=		../libntp/libntp.a
ntp_keygen_SOURCES = ntp-keygen.c ntp-keygen-opts.c ntp-keygen-opts.h
ntp_keygen_LDADD= version.o $(LIBOPTS_LDADD) ../libntp/libntp.a @LCRYPTO@

ETAGS_ARGS=	Makefile.am
#EXTRA_DIST=	README TAGS
EXTRA_DIST=	ntp-keygen-opts.def ntp-keygen.1 ntp-keygen-opts.texi ntp-keygen-opts.menu
BUILT_SOURCES=	ntp-keygen-opts.c ntp-keygen-opts.h
CLEANFILES=
DISTCLEANFILES=	.version version.c
noinst_DATA=	$(srcdir)/ntp-keygen-opts.texi $(srcdir)/ntp-keygen-opts.menu
man_MANS=	$(srcdir)/ntp-keygen.1
run_ag=		cd $(srcdir) &&	env PATH="$(abs_builddir):$(PATH)"	\
		autogen -L ../include --writable
std_def_list=	$(top_srcdir)/include/debug-opt.def		\
		$(top_srcdir)/include/autogen-version.def 	\
		$(top_srcdir)/include/copyright.def 		\
		$(top_srcdir)/include/homerc.def 		\
		$(top_srcdir)/include/version.def

$(srcdir)/ntp-keygen-opts.h: $(srcdir)/ntp-keygen-opts.c
	@: do-nothing action to avoid default SCCS get, .h built with .c
	
$(srcdir)/ntp-keygen-opts.c: $(srcdir)/ntp-keygen-opts.def $(std_def_list)
	$(run_ag) ntp-keygen-opts.def

$(srcdir)/ntp-keygen.1: $(srcdir)/ntp-keygen-opts.def $(std_def_list)
	$(run_ag) -Tagman1.tpl -bntp-keygen ntp-keygen-opts.def

$(srcdir)/ntp-keygen-opts.menu: $(srcdir)/ntp-keygen-opts.texi
	@: do-nothing action to avoid default SCCS get, .menu built with .texi
	
$(srcdir)/ntp-keygen-opts.texi: $(srcdir)/ntp-keygen-opts.def $(std_def_list)
	$(run_ag) -Taginfo.tpl -DLEVEL=section ntp-keygen-opts.def
	$(top_srcdir)/scripts/check--help $@


jitter_SOURCES=	jitter.c jitter.h
jitter_LDADD=

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

kern.o: kern.c
	$(COMPILE) -DHAVE_TIMEX_H -c kern.c

$(top_srcdir)/version :
	cd $(top_srcdir) && $(MAKE) version

version.o: $(ntpq_OBJECTS) ../libntp/libntp.a Makefile $(top_srcdir)/version
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntp-keygen
	$(COMPILE) -c version.c

include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/depsver.mf
