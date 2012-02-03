AUTOMAKE_OPTIONS=	../util/ansi2knr

bindir=		${exec_prefix}/${BINSUBDIR}
bin_PROGRAMS=	ntpdc
EXTRA_PROGRAMS=	ntpdc-layout
EXTRA_DATA=	check-layout
BUILT_SOURCES=	@MAKE_CHECK_LAYOUT@
AM_CPPFLAGS=	-I$(top_srcdir)/include $(LIBOPTS_CFLAGS)
# LDADD might need RESLIB and ADJLIB
ntpdc_LDADD=	version.o @READLINE_LIBS@ $(LIBOPTS_LDADD) ../libntp/libntp.a
# ntpdc-layout doesn't need any additional libraries at all
ntpdc_layout_LDADD=
DISTCLEANFILES=	.version version.c
CLEANFILES=	check-layout layout.here nl.c ntpdc-layout
noinst_HEADERS=	ntpdc.h
ETAGS_ARGS= 	Makefile.am
EXTRA_DIST=	nl_in.c nl.pl layout.std	\
		ntpdc-opts.def ntpdc.1 ntpdc-opts.texi ntpdc-opts.menu
BUILT_SOURCES+=	ntpdc-opts.c ntpdc-opts.h ntpdc.1 ntpdc-opts.texi	\
		ntpdc-opts.menu
man_MANS=	ntpdc.1
run_ag=         cd $(srcdir) && autogen -L ../include --writable
std_def_list=	$(top_srcdir)/include/debug-opt.def		\
		$(top_srcdir)/include/autogen-version.def 	\
		$(top_srcdir)/include/copyright.def 		\
		$(top_srcdir)/include/homerc.def 		\
		$(top_srcdir)/include/version.def

$(srcdir)/ntpdc-opts.h: $(srcdir)/ntpdc-opts.c
$(srcdir)/ntpdc-opts.c: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(run_ag) ntpdc-opts.def

$(srcdir)/ntpdc.1: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(run_ag) -Tagman1.tpl -bntpdc ntpdc-opts.def

$(srcdir)/ntpdc-opts.texi $(srcdir)/ntpdc-opts.menu: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(run_ag) -Taginfo.tpl -DLEVEL=section ntpdc-opts.def

ntpdc_SOURCES = ntpdc.c ntpdc_ops.c ntpdc-opts.c ntpdc-opts.h

ntpdc-layout.o: nl.c

layout.here: ntpdc-layout
	./ntpdc-layout > $@

check-layout: ntpdc-layout layout.std layout.here
	cmp $(srcdir)/layout.std layout.here && echo stamp > $@

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

$(top_srcdir)/version :
	cd $(top_srcdir) && $(MAKE) version

version.o: $(ntpdc_OBJECTS) ../libntp/libntp.a Makefile $(top_srcdir)/version
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntpdc
	$(COMPILE) -c version.c

# I ran nl_in.c (attached, to be installed into ntpdc) through
# $(CC) -E nl_in.c | nl.pl > nl.c
nl.c: nl_in.c nl.pl Makefile.am
	$(CC) -E $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(srcdir)/nl_in.c | ./nl.pl > nl.c

include ../bincheck.mf
