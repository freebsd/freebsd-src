# Makefile.am for JMK's SNTP, by Harlan Stenn

NULL =

## LIBOPTS_CHECK_NOBUILD works with Automake 1.10 now
AUTOMAKE_OPTIONS = foreign 1.10
ACLOCAL_AMFLAGS = -I ../m4 -I libopts/m4

AM_CPPFLAGS = $(LIBOPTS_CFLAGS) -I$(top_srcdir)/../include \
	-I$(top_srcdir)/../lib/isc/include \
	-I$(top_srcdir)/../lib/isc/nothreads/include \
	-I$(top_srcdir)/../lib/isc/unix/include

LDADD =	$(LIBOPTS_LDADD) $(LIBM) ../libntp/libntp.a @LCRYPTO@

run_ag =	cd $(srcdir) &&	env PATH="$(abs_builddir):$(PATH)"	\
		autogen -L ../include --writable

if NTP_BINSUBDIR_IS_BIN
bin_PROGRAMS =	sntp
else
sbin_PROGRAMS =	sntp
endif

CLEANFILES = check-COPYRIGHT-submake

SUBDIRS =
if NEED_LIBOPTS
SUBDIRS += libopts
endif

sntp_SOURCES =		\
	crypto.c	\
	crypto.h	\
	data_formats.h	\
	header.h	\
	kod_management.c \
	kod_management.h \
	log.c		\
	log.h		\
	main.c		\
	main.h		\
	networking.c	\
	networking.h	\
	sntp.c		\
	sntp-opts.c	\
	sntp-opts.h	\
	utilities.c	\
	utilities.h	\
	$(NULL)

EXTRA_DIST =				\
	$(srcdir)/autogen-version.def	\
	bincheck.mf			\
	$(srcdir)/COPYRIGHT		\
	deps-ver			\
	depsver.mf			\
	sntp-opts.def			\
	sntp-opts.menu			\
	sntp-opts.texi			\
	sntp.1				\
	sntp.html			\
	sntp.texi			\
	$(srcdir)/version.def		\
	$(srcdir)/version.m4		\
	$(srcdir)/version.texi		\
	$(NULL)

OLD_EXTRA_DIST=		\
		autogen-version.def version.def version.m4 version.texi

BUILT_SOURCES=				\
	check-autogen-version.def	\
	check-version.def		\
	check-version.m4		\
	check-version.texi		\
	$(srcdir)/COPYRIGHT		\
	libtool				\
	$(srcdir)/sntp-opts.c		\
	$(srcdir)/sntp-opts.h		\
	$(NULL)

man_MANS=	$(srcdir)/sntp.1

# HMS: Real Soon Now...
#info_TEXINFOS=	sntp.texi
#sntp_TEXINFOS=	sntp-opts.texi

noinst_DATA=				\
	$(srcdir)/sntp.html		\
	$(srcdir)/sntp-opts.texi	\
	$(srcdir)/sntp-opts.menu	\
	$(NULL)

FRC:
	@: do-nothing action to prevent default SCCS get
	@: FRC "force" depends on nothing and is not a file, so is
	@: always out-of-date causing targets which depend on it to
	@: similarly always be outdated causing their rules to fire
	@: each time they or a dependent is built.

check-autogen-version.def: FRC
	@cd $(srcdir)							\
	&& test -r ../include/autogen-version.def			\
	&& ( if cmp -s ../include/autogen-version.def autogen-version.def; \
	   then : ;							\
	   else cp ../include/autogen-version.def autogen-version.def;	 \
		echo "Installing new sntp/autogen-version.def file";	\
	   fi )

$(srcdir)/autogen-version.def:
	$(MAKE) check-autogen-version.def

check-version.def: FRC
	@cd $(srcdir)					\
	&& test -r ../include/version.def		\
	&& ( if cmp -s ../include/version.def version.def;	\
	   then : ;						\
	   else cp ../include/version.def version.def;		\
		echo "Installing new sntp/version.def file";	\
	   fi )

$(srcdir)/version.def:
	$(MAKE) check-version.def

check-version.m4: FRC
	@cd $(srcdir)					\
	&& test -r ../version.m4			\
	&& ( if cmp -s ../version.m4 version.m4;	\
	   then : ;					\
	   else cp ../version.m4 version.m4;		\
		echo "Installing new sntp/version.m4 file";	\
	   fi )

$(srcdir)/version.m4:
	$(MAKE) check-version.m4

check-version.texi: FRC
	@cd $(srcdir)					\
	&& test -r ../include/version.texi			\
	&& ( if cmp -s ../include/version.texi version.texi;	\
	   then : ;					\
	   else cp ../include/version.texi version.texi;	\
		echo "Installing new sntp/version.texi file";	\
	   fi )

$(srcdir)/version.texi:
	$(MAKE) check-version.texi

$(srcdir)/../COPYRIGHT:
	cd .. && $(MAKE) COPYRIGHT-please

check-COPYRIGHT-submake: $(srcdir)/../COPYRIGHT
	@cmp -s $(srcdir)/../COPYRIGHT $(srcdir)/COPYRIGHT	\
	 || { cp $(srcdir)/../COPYRIGHT $(srcdir)		\
	      && echo 'updated sntp/COPYRIGHT installed' ;}
	@echo 'submake avoided if this file is newer than parent COPYRIGHT' > $@

check-COPYRIGHT: FRC
	[ ! -r $(srcdir)/../COPYRIGHT ]					\
	|| [ check-COPYRIGHT-submake -nt $(srcdir)/../COPYRIGHT ]	\
	|| $(MAKE) check-COPYRIGHT-submake

$(srcdir)/COPYRIGHT: check-COPYRIGHT
	@: do-nothing action to prevent any default

$(srcdir)/sntp-opts.h: $(srcdir)/sntp-opts.c
	@: do-nothing action to avoid default SCCS get, .h built with .c
	
$(srcdir)/sntp-opts.c: $(srcdir)/sntp-opts.def $(srcdir)/version.def $(srcdir)/../include/copyright.def
	$(run_ag) sntp-opts.def

$(srcdir)/sntp.1: $(srcdir)/sntp-opts.def $(srcdir)/version.def $(srcdir)/../include/copyright.def
	$(run_ag) -Tagman1.tpl -bsntp sntp-opts.def

$(srcdir)/sntp-opts.menu: $(srcdir)/sntp-opts.texi
	@: do-nothing action to avoid default SCCS get, .menu built with .texi
	
$(srcdir)/sntp-opts.texi: $(srcdir)/sntp-opts.def $(srcdir)/version.def $(srcdir)/../include/copyright.def
	$(run_ag) -Taginfo.tpl -DLEVEL=section sntp-opts.def
	$(top_srcdir)/../scripts/check--help $@

$(srcdir)/sntp.html: $(srcdir)/sntp-opts.menu $(srcdir)/sntp-opts.texi $(srcdir)/sntp.texi $(srcdir)/version.texi
	cd $(srcdir) && ( makeinfo --force --html --no-split -o sntp.html sntp.texi || true )

../libntp/libntp.a:
	cd ../libntp && $(MAKE) libntp.a

libtool: $(LIBTOOL_DEPS)
	./config.status --recheck

include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/depsver.mf
