# $Id: Makefile.am,v 1.7 1999/04/09 18:22:45 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

bin_PROGRAMS = kauth
bin_SCRIPTS = ksrvtgt
libexec_PROGRAMS = kauthd

EXTRA_DIST = zrefresh ksrvtgt.in

kauth_SOURCES = \
	kauth.c \
	kauth.h \
	rkinit.c \
	marshall.c \
	encdata.c

kauthd_SOURCES = \
	kauthd.c \
	kauth.h \
	marshall.c \
	encdata.c

ksrvtgt: ksrvtgt.in
	sed -e "s!%bindir%!$(bindir)!" $(srcdir)/ksrvtgt.in > $@
	chmod +x $@

install-exec-local:
	if test -f $(bindir)/zrefresh -o -r  $(bindir)/zrefresh; then \
	  true; \
	else \
	  $(INSTALL_PROGRAM) $(srcdir)/zrefresh $(bindir)/`echo zrefresh | sed '$(transform)'`; \
	fi

LDADD = \
	$(LIB_kafs)				\
	$(LIB_krb5)				\
	$(LIB_krb4)				\
	$(top_builddir)/lib/des/libdes.la	\
	$(LIB_roken)
