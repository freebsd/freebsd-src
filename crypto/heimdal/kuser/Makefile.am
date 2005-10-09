# $Id: Makefile.am,v 1.31 2003/03/18 13:15:27 lha Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4) $(INCLUDE_des) -I$(srcdir)/../lib/krb5

man_MANS = kinit.1 klist.1 kdestroy.1 kgetcred.1

bin_PROGRAMS = kinit klist kdestroy kgetcred

noinst_PROGRAMS = kverify kdecode_ticket generate-requests

kinit_LDADD = \
	$(LIB_kafs) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_krb4) \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

kdestroy_LDADD	= $(kinit_LDADD)

klist_LDADD	= $(kinit_LDADD)

LDADD = \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

# make sure install-exec-hook doesn't have any commands in Makefile.am.common
install-exec-hook:
	(cd $(DESTDIR)$(bindir) && rm -f kauth && $(LN_S) kinit kauth)
