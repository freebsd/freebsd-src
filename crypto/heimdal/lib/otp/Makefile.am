# $Id: Makefile.am,v 1.16 2001/01/30 01:54:48 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_krb4)

noinst_PROGRAMS = otptest

otptest_LDADD = libotp.la \
	$(LIB_des) \
	$(LIB_roken) \
	$(DBLIB)

include_HEADERS = otp.h

lib_LTLIBRARIES = libotp.la
libotp_la_LDFLAGS = -version-info 1:2:1

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
	roken_rename.h
