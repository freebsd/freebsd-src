NULL=
AUTOMAKE_OPTIONS=	

if NTP_BINSUBDIR_IS_BIN
bin_PROGRAMS=	ntpq
else
sbin_PROGRAMS=	ntpq
endif

AM_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/lib/isc/include \
	-I$(top_srcdir)/lib/isc/nothreads/include \
	-I$(top_srcdir)/lib/isc/unix/include $(LIBOPTS_CFLAGS)

# LDADD might need RESLIB and ADJLIB
ntpq_LDADD=	version.o $(LIBOPTS_LDADD) ../libntp/libntp.a \
		@EDITLINE_LIBS@ @LCRYPTO@
noinst_HEADERS=	ntpq.h
noinst_LIBRARIES=	libntpq.a
libntpq_a_CFLAGS=	-DNO_MAIN_ALLOWED -DBUILD_AS_LIB
CLEANFILES=
DISTCLEANFILES=	.version version.c
ETAGS_ARGS=	Makefile.am
EXTRA_DIST=	ntpq-opts.def ntpq.1 ntpq-opts.texi ntpq-opts.menu
BUILT_SOURCES=	ntpq-opts.c ntpq-opts.h
man_MANS=	$(srcdir)/ntpq.1
noinst_DATA=	$(srcdir)/ntpq-opts.texi $(srcdir)/ntpq-opts.menu
run_ag=		cd $(srcdir) &&	env PATH="$(abs_builddir):$(PATH)"	\
		autogen -L ../include --writable
std_def_list=	$(top_srcdir)/include/debug-opt.def		\
		$(top_srcdir)/include/autogen-version.def 	\
		$(top_srcdir)/include/copyright.def 		\
		$(top_srcdir)/include/homerc.def 		\
		$(top_srcdir)/include/version.def
ntpq_SOURCES=	ntpq.c ntpq-subs.c ntpq-opts.c ntpq-opts.h

libntpq_a_SOURCES =	libntpq.c libntpq.h libntpq_subs.c 

$(srcdir)/ntpq-opts.h: $(srcdir)/ntpq-opts.c
	@: do-nothing action to avoid default SCCS get, .h built with .c
	
$(srcdir)/ntpq-opts.c: $(srcdir)/ntpq-opts.def $(std_def_list)
	$(run_ag) ntpq-opts.def

$(srcdir)/ntpq.1: $(srcdir)/ntpq-opts.def $(std_def_list)
	$(run_ag) -Tagman1.tpl -bntpq ntpq-opts.def

$(srcdir)/ntpq-opts.menu: $(srcdir)/ntpq-opts.texi
	@: do-nothing action to avoid default SCCS get, .menu built with .texi
	
$(srcdir)/ntpq-opts.texi: $(srcdir)/ntpq-opts.def $(std_def_list)
	$(run_ag) -Taginfo.tpl -DLEVEL=section ntpq-opts.def
	$(top_srcdir)/scripts/check--help $@

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

$(top_srcdir)/version :
	cd $(top_srcdir) && $(MAKE) version

version.o: $(ntpq_OBJECTS) ../libntp/libntp.a Makefile $(top_srcdir)/version
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntpq
	$(COMPILE) -c version.c

include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/depsver.mf
