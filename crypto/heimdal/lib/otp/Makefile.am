# $Id: Makefile.am,v 1.22 2002/08/13 14:02:54 joda Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_des) $(ROKEN_RENAME)

noinst_PROGRAMS = otptest

check_PROGRAMS = otptest

otptest_LDADD = libotp.la

include_HEADERS = otp.h

lib_LTLIBRARIES = libotp.la
libotp_la_LDFLAGS = -version-info 1:3:1
libotp_la_LIBADD  = $(LIB_des) $(LIB_roken) $(LIB_NDBM)

if HAVE_DB3
ndbm_wrap = ndbm_wrap.c ndbm_wrap.h
else
ndbm_wrap =
endif

libotp_la_SOURCES = \
	otp.c \
	otp_challenge.c \
	otp_db.c \
	otp_md.c \
	otp_parse.c \
	otp_print.c \
	otp_verify.c \
	otp_locl.h \
	otp_md.h \
	roken_rename.h \
	$(ndbm_wrap) \
	$(ROKEN_SRCS)

if do_roken_rename
ROKEN_SRCS = snprintf.c strcasecmp.c strncasecmp.c strlwr.c
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
