# $Id: Makefile.am 22285 2007-12-13 20:40:57Z lha $

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_hcrypto) -I$(srcdir)/../lib/krb5

man_MANS = \
	kinit.1 \
	klist.1 \
	kdestroy.1 \
	kgetcred.1 \
	kimpersonate.1

SLC = $(top_builddir)/lib/sl/slc

bin_PROGRAMS = kinit klist kdestroy kgetcred
libexec_PROGRAMS = kdigest kimpersonate

noinst_PROGRAMS = kverify kdecode_ticket generate-requests copy_cred_cache

kinit_LDADD = \
	$(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

kdestroy_LDADD	= $(kinit_LDADD)

klist_LDADD	= $(kinit_LDADD)

kimpersonate_LDADD = $(kinit_LDADD)

dist_kdigest_SOURCES = kdigest.c
nodist_kdigest_SOURCES = kdigest-commands.c

kdigest_LDADD = \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(top_builddir)/lib/sl/libsl.la \
	$(LIB_roken)

$(kdigest_OBJECTS): kdigest-commands.h

CLEANFILES = kdigest-commands.h kdigest-commands.c

kdigest-commands.c kdigest-commands.h: kdigest-commands.in
	$(SLC) $(srcdir)/kdigest-commands.in

LDADD = \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

# make sure install-exec-hook doesn't have any commands in Makefile.am.common
install-exec-hook:
	(cd $(DESTDIR)$(bindir) && rm -f kauth && $(LN_S) kinit kauth)

EXTRA_DIST = $(man_MANS) kuser_locl.h kdigest-commands.in copy_cred_cache.1

