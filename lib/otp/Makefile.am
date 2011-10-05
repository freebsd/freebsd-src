# $Id$

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_hcrypto) $(ROKEN_RENAME)

noinst_PROGRAMS = otptest

check_PROGRAMS = otptest

otptest_LDADD = libotp.la

include_HEADERS = otp.h

lib_LTLIBRARIES = libotp.la
libotp_la_LDFLAGS = -version-info 1:5:1
libotp_la_LIBADD  = $(LIB_hcrypto) $(LIB_roken)

if HAVE_DB1
ndbm_wrap = ndbm_wrap.c ndbm_wrap.h
libotp_la_LIBADD += $(LIB_db_create)
else
if HAVE_DB3
ndbm_wrap = ndbm_wrap.c ndbm_wrap.h
libotp_la_LIBADD += $(LIB_db_create)
else
ndbm_wrap =
libotp_la_LIBADD += $(LIB_NDBM)
endif
endif

dist_libotp_la_SOURCES = \
	otp.c \
	otp_challenge.c \
	otp_db.c \
	otp_md.c \
	otp_parse.c \
	otp_print.c \
	otp_verify.c \
	otp_locl.h \
	otp_md.h \
	roken_rename.h

nodist_libotp_la_SOURCES = $(ndbm_wrap) $(ROKEN_SRCS)

libotp_la_DEPENDENCIES = version-script.map

if do_roken_rename
ROKEN_SRCS = snprintf.c strcasecmp.c strncasecmp.c strlwr.c strlcpy.c strlcat.c
endif

if versionscript
libotp_la_LDFLAGS += $(LDFLAGS_VERSION_SCRIPT)$(srcdir)/version-script.map
endif

$(libotp_la_OBJECTS): $(ndbm_wrap)

ndbm_wrap.c:
	$(LN_S) $(srcdir)/../roken/ndbm_wrap.c .
ndbm_wrap.h:
	(echo '#define dbm_rename(X) __otp_ ## X'; cat $(srcdir)/../roken/ndbm_wrap.h) > ndbm_wrap.h


snprintf.c:
	$(LN_S) $(srcdir)/../roken/snprintf.c .
strcasecmp.c:
	$(LN_S) $(srcdir)/../roken/strcasecmp.c .
strncasecmp.c:
	$(LN_S) $(srcdir)/../roken/strncasecmp.c .
strlwr.c:
	$(LN_S) $(srcdir)/../roken/strlwr.c .
strlcpy.c:
	$(LN_S) $(srcdir)/../roken/strlcpy.c .
strlcat.c:
	$(LN_S) $(srcdir)/../roken/strlcat.c .

CLEANFILES = \
	ndbm_wrap.c \
	ndbm_wrap.h \
	snprintf.c \
	strcasecmp.c \
	strlcat.c \
	strlcpy.c \
	strlwr.c \
	strncasecmp.c

EXTRA_DIST = NTMakefile version-script.map
