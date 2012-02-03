# Makefile.am, by Harlan Stenn, from:
# Makefile for sntp
# N.M. Maclaren, October 2000.

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS= -I libopts/m4

AM_CPPFLAGS=	$(LIBOPTS_CFLAGS)
LDADD=	$(LIBOPTS_LDADD)

NULL=

bindir=	${exec_prefix}/${BINSUBDIR}
bin_PROGRAMS=	sntp
#run_ag=         cd $(srcdir) && autogen --writable -L $(top_srcdir)/include
run_ag=         cd $(srcdir) && autogen --writable

SUBDIRS=
if NEED_LIBOPTS
SUBDIRS+= libopts
endif
SUBDIRS+= .

sntp_SOURCES = \
	header.h \
	internet.c \
	internet.h \
	kludges.h \
	main.c \
	sntp-opts.c \
	sntp-opts.h \
	socket.c \
	timing.c \
	unix.c \
	$(NULL)

dist_man_MANS=	sntp.1

EXTRA_DIST=	bincheck.mf	\
		COPYRIGHT	\
		sntp-opts.def sntp.1 sntp-opts.texi sntp-opts.menu	\
		autogen-version.def version.def
BUILT_SOURCES=	check-autogen-version.def check-version.def	\
		sntp-opts.c sntp-opts.h sntp.1 sntp-opts.texi sntp-opts.menu
man_MANS=	sntp.1


FRC:
check-autogen-version.def: FRC
	@cd $(srcdir)							\
	&& test -r ../include/autogen-version.def			\
	&& ( if cmp -s ../include/autogen-version.def autogen-version.def; \
	   then : ;							\
	   else cp ../include/autogen-version.def autogen-version.def;	 \
		echo "Installing new sntp/autogen-version.def file";	\
	   fi )

check-version.def: FRC
	@cd $(srcdir)					\
	&& test -r ../include/version.def		\
	&& ( if cmp -s ../include/version.def version.def;	\
	   then : ;						\
	   else cp ../include/version.def version.def;		\
		echo "Installing new sntp/version.def file";	\
	   fi )

$(srcdir)/sntp-opts.h: $(srcdir)/sntp-opts.c
$(srcdir)/sntp-opts.c: $(srcdir)/sntp-opts.def $(srcdir)/version.def
	$(run_ag) sntp-opts.def

$(srcdir)/sntp.1: $(srcdir)/sntp-opts.def $(srcdir)/version.def
	$(run_ag) -Tagman1.tpl -bsntp sntp-opts.def

$(srcdir)/sntp-opts.texi $(srcdir)/sntp-opts.menu: $(srcdir)/sntp-opts.def $(srcdir)/version.def
	$(run_ag) -Taginfo.tpl -DLEVEL=section sntp-opts.def

include bincheck.mf
