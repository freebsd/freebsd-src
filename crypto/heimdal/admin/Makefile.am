# $Id: Makefile.am,v 1.33 2000/12/16 00:16:45 joda Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_readline)

man_MANS = ktutil.8

sbin_PROGRAMS = ktutil

ktutil_SOURCES = add.c				\
		 change.c			\
		 copy.c				\
		 get.c				\
		 ktutil.c			\
		 list.c				\
		 purge.c			\
		 remove.c

LDADD = \
	$(top_builddir)/lib/kadm5/libkadm5clnt.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_des) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(top_builddir)/lib/sl/libsl.la \
	$(LIB_readline) \
	$(LIB_roken)
