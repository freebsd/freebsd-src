# $Id: Makefile.am,v 1.20 2001/09/02 19:37:12 assar Exp $

include $(top_srcdir)/Makefile.am.common

INCLUDES += $(INCLUDE_des)

noinst_PROGRAMS = otptest

check_PROGRAMS = otptest

otptest_LDADD = libotp.la

include_HEADERS = otp.h

lib_LTLIBRARIES = libotp.la
libotp_la_LDFLAGS = -version-info 1:3:1
libotp_la_LIBADD  = $(LIB_des) $(LIB_roken) $(LIB_NDBM)

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
