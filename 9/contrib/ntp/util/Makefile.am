#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS=	ansi2knr

bin_PROGRAMS=	@MAKE_NTPTIME@ @MAKE_TICKADJ@ @MAKE_TIMETRIM@ \
		ntp-keygen
EXTRA_PROGRAMS=	audio-pcm byteorder hist jitter kern longsize \
	ntptime pps-api precision sht testrs6000 tg tickadj timetrim

AM_CPPFLAGS=	-I$(top_srcdir)/include $(LIBOPTS_CFLAGS)
# LDADD might need RESLIB and ADJLIB
LDADD=		../libntp/libntp.a
ntp_keygen_SOURCES = ntp-keygen.c ntp-keygen-opts.c ntp-keygen-opts.h
ntp_keygen_LDADD=	@LCRYPTO@ $(LIBOPTS_LDADD) ../libntp/libntp.a

ETAGS_ARGS=	Makefile.am
#EXTRA_DIST=	README TAGS
EXTRA_DIST=	ntp-keygen-opts.def ntp-keygen.1 ntp-keygen-opts.texi ntp-keygen-opts.menu
BUILT_SOURCES=	ntp-keygen-opts.c ntp-keygen-opts.h ntp-keygen.1	\
		ntp-keygen-opts.texi ntp-keygen-opts.menu
man_MANS=	ntp-keygen.1
run_ag=         cd $(srcdir) && autogen -L ../include --writable
std_def_list=	$(top_srcdir)/include/debug-opt.def		\
		$(top_srcdir)/include/autogen-version.def 	\
		$(top_srcdir)/include/copyright.def 		\
		$(top_srcdir)/include/homerc.def 		\
		$(top_srcdir)/include/version.def

$(srcdir)/ntp-keygen-opts.h: $(srcdir)/ntp-keygen-opts.c
$(srcdir)/ntp-keygen-opts.c: $(srcdir)/ntp-keygen-opts.def $(std_def_list)
	$(run_ag) ntp-keygen-opts.def

$(srcdir)/ntp-keygen.1: $(srcdir)/ntp-keygen-opts.def $(std_def_list)
	$(run_ag) -Tagman1.tpl -bntp-keygen ntp-keygen-opts.def

$(srcdir)/ntp-keygen-opts.texi $(srcdir)/ntp-keygen-opts.menu: $(srcdir)/ntp-keygen-opts.def $(std_def_list)
	$(run_ag) -Taginfo.tpl -DLEVEL=section ntp-keygen-opts.def


jitter_SOURCES=	jitter.c jitter.h
jitter_LDADD=

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

kern.o: kern.c
	$(COMPILE) -DHAVE_TIMEX_H -c kern.c
