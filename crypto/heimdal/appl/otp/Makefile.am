# $Id: Makefile.am,v 1.9 2000/11/15 22:51:09 assar Exp $

include $(top_srcdir)/Makefile.am.common

bin_PROGRAMS = otp otpprint
bin_SUIDS = otp
otp_SOURCES = otp.c otp_locl.h
otpprint_SOURCES = otpprint.c otp_locl.h

man_MANS = otp.1  otpprint.1

LDADD = \
	$(top_builddir)/lib/otp/libotp.la \
	$(LIB_des) \
	$(LIB_roken) \
	$(DBLIB)
