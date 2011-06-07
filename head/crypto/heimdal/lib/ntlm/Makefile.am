# $Id: Makefile.am 22045 2007-11-11 08:57:47Z lha $

include $(top_srcdir)/Makefile.am.common

lib_LTLIBRARIES	= libheimntlm.la

include_HEADERS = heimntlm.h heimntlm-protos.h

libheimntlm_la_SOURCES	= ntlm.c heimntlm.h

libheimntlm_la_LDFLAGS = -version-info 1:0:1

if versionscript
libheimntlm_la_LDFLAGS += $(LDFLAGS_VERSION_SCRIPT)$(srcdir)/version-script.map
endif
$(libheimntlm_la_OBJECTS): $(srcdir)/version-script.map

libheimntlm_la_LIBADD = \
	../krb5/libkrb5.la \
	$(LIBADD_roken)

$(srcdir)/heimntlm-protos.h:
	cd $(srcdir) && perl ../../cf/make-proto.pl -q -P comment -o heimntlm-protos.h $(libheimntlm_la_SOURCES) || rm -f heimntlm-protos.h

$(libheimntlm_la_OBJECTS): $(srcdir)/heimntlm-protos.h


TESTS = test_ntlm

check_PROGRAMS = test_ntlm

LDADD = libheimntlm.la $(LIB_roken)

EXTRA_DIST = version-script.map
