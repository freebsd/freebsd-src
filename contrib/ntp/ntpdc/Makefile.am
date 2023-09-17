NULL=

bin_PROGRAMS=	$(NTPDC_DB)
libexec_PROGRAMS=	$(NTPDC_DL)
sbin_PROGRAMS=	$(NTPDC_DS)

EXTRA_PROGRAMS=	ntpdc ntpdc-layout
EXTRA_DATA=	check-layout
BUILT_SOURCES=	@MAKE_CHECK_LAYOUT@ ntpdc-opts.c ntpdc-opts.h

AM_CFLAGS = $(CFLAGS_NTP)
AM_CFLAGS += $(NTP_HARD_CFLAGS)

AM_CPPFLAGS  = $(NTP_INCS)
AM_CPPFLAGS += $(LIBOPTS_CFLAGS)
AM_CPPFLAGS += $(CPPFLAGS_NTP)
AM_CPPFLAGS += $(NTP_HARD_CPPFLAGS)

AM_LDFLAGS = $(LDFLAGS_NTP)
AM_LDFLAGS += $(NTP_HARD_LDFLAGS)

# LDADD might need RESLIB and ADJLIB
ntpdc_LDADD  = $(LIBOPTS_LDADD)
ntpdc_LDADD += ../libntp/libntp.a $(LDADD_LIBNTP) $(LIBM)
ntpdc_LDADD += $(PTHREAD_LIBS) $(EDITLINE_LIBS)
ntpdc_LDADD += $(LDADD_NTP)
# ntpdc-layout doesn't need any additional libraries at all
ntpdc_layout_LDADD=

DISTCLEANFILES =	\
	$(man_MANS)	\
	config.log	\
	$(NULL)

CLEANFILES =		\
	check-layout	\
	layout.here	\
	nl.c		\
	ntpdc-layout	\
	.version	\
	version.c	\
	$(NULL)

noinst_HEADERS=	ntpdc.h
ETAGS_ARGS= 	Makefile.am
EXTRA_DIST=			\
	invoke-ntpdc.menu	\
	invoke-ntpdc.texi	\
	layout.std		\
	nl_in.c			\
	nl.pl			\
	ntpdc-opts.def		\
	ntpdc.1ntpdcman		\
	ntpdc.1ntpdcmdoc	\
	ntpdc.man.in		\
	ntpdc.mdoc.in		\
	ntpdc.html		\
	ntpdc.texi		\
	$(NULL)

man1_MANS=
man8_MANS=
man_MANS=	ntpdc.$(NTPDC_MS)

## HMS: Real Soon Now...
##info_TEXINFOS=	ntpdc.texi
##ntpdc_TEXINFOS=	ntpdc-opts.texi

html_DATA=				\
	$(srcdir)/ntpdc.html		\
	$(NULL)

noinst_DATA=				\
	$(srcdir)/invoke-ntpdc.menu	\
	$(srcdir)/invoke-ntpdc.texi	\
	$(srcdir)/ntpdc.man.in		\
	$(srcdir)/ntpdc.mdoc.in		\
	$(NULL)

install-data-local: install-html

run_ag=		cd $(srcdir) &&	env PATH="$(abs_builddir):$(PATH)" AUTOGEN_DNE_DATE=-D	\
		autogen -L ../sntp/include -L ../sntp/ag-tpl --writable

std_def_list =							\
	$(top_srcdir)/sntp/include/debug-opt.def		\
	$(top_srcdir)/sntp/include/autogen-version.def 		\
	$(top_srcdir)/sntp/include/copyright.def 		\
	$(top_srcdir)/sntp/include/homerc.def 			\
	$(top_srcdir)/sntp/include/ntp.lic			\
	$(top_srcdir)/sntp/include/version.def			\
	$(NULL)

$(srcdir)/ntpdc-opts.h: $(srcdir)/ntpdc-opts.c
	@: do-nothing action to avoid default SCCS get, .h built with .c
	
$(srcdir)/ntpdc-opts.c: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(AM_V_GEN)$(run_ag) ntpdc-opts.def

###

$(srcdir)/ntpdc.1ntpdcman: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(AM_V_GEN)$(run_ag) -DMAN_SECTION=1ntpdcman -Tagman-cmd.tpl ntpdc-opts.def

$(srcdir)/ntpdc.man.in: $(srcdir)/ntpdc.1ntpdcman $(top_srcdir)/sntp/scripts/mansec2subst.sed
	sed -f $(top_srcdir)/sntp/scripts/mansec2subst.sed $(srcdir)/ntpdc.1ntpdcman > $(srcdir)/ntpdc.man.in+
	mv $(srcdir)/ntpdc.man.in+ $(srcdir)/ntpdc.man.in

###

$(srcdir)/ntpdc.1ntpdcmdoc: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(AM_V_GEN)$(run_ag) -DMAN_SECTION=1ntpdcmdoc -Tagmdoc-cmd.tpl ntpdc-opts.def

$(srcdir)/ntpdc.mdoc.in: $(srcdir)/ntpdc.1ntpdcmdoc $(top_srcdir)/sntp/scripts/mansec2subst.sed
	sed -f $(top_srcdir)/sntp/scripts/mansec2subst.sed $(srcdir)/ntpdc.1ntpdcmdoc > $(srcdir)/ntpdc.mdoc.in+
	mv $(srcdir)/ntpdc.mdoc.in+ $(srcdir)/ntpdc.mdoc.in

###

ntpdc.$(NTPDC_MS): $(srcdir)/ntpdc.$(MANTAGFMT).in $(top_builddir)/config.status
	$(top_builddir)/config.status --file=ntpdc.$(NTPDC_MS)+:$(srcdir)/ntpdc.$(MANTAGFMT).in
	mv ntpdc.$(NTPDC_MS)+ ntpdc.$(NTPDC_MS)

###

$(srcdir)/invoke-ntpdc.menu: $(srcdir)/invoke-ntpdc.texi
	@: do-nothing action to avoid default SCCS get, .menu built with .texi
	
$(srcdir)/invoke-ntpdc.texi: $(srcdir)/ntpdc-opts.def $(std_def_list)
	$(AM_V_GEN)$(run_ag) -Tagtexi-cmd.tpl -DLEVEL=section ntpdc-opts.def
	$(top_srcdir)/scripts/build/check--help $@

$(srcdir)/ntpdc.html: $(srcdir)/invoke-ntpdc.texi $(srcdir)/ntpdc.texi $(top_srcdir)/sntp/include/version.texi
	cd $(srcdir) && ( makeinfo --force --html --no-split -I ../sntp -o ntpdc.html ntpdc.texi || true )

ntpdc_SOURCES = ntpdc.c ntpdc_ops.c ntpdc-opts.c ntpdc-opts.h
nodist_ntpdc_SOURCES = version.c

ntpdc-layout.o: nl.c

layout.here: ntpdc-layout
	$(AM_V_at)./ntpdc-layout > $@

check-layout: ntpdc-layout $(srcdir)/layout.std layout.here
	$(AM_V_at)cmp $(srcdir)/layout.std layout.here && echo stamp > $@

version.c: Makefile $(top_srcdir)/sntp/scm-rev
	$(AM_V_GEN)env CSET=`cat $(top_srcdir)/sntp/scm-rev` $(top_builddir)/scripts/build/mkver ntpdc
	
# I ran nl_in.c (attached, to be installed into ntpdc) through
# $(CC) -E nl_in.c | nl.pl > nl.c
nl.c: nl_in.c nl.pl Makefile.am ../config.h ../include/ntp_request.h ntpdc.h
	$(AM_V_GEN)$(CPP) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(srcdir)/nl_in.c | ./nl.pl > $@


include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/check-libopts.mf
include $(top_srcdir)/check-libntp.mf
include $(top_srcdir)/check-scm-rev.mf
include $(top_srcdir)/depsver.mf
include $(top_srcdir)/includes.mf
