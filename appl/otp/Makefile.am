# $Id$

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_hcrypto)

bin_PROGRAMS = otp otpprint
bin_SUIDS = otp
otp_SOURCES = otp.c otp_locl.h
otp_LDADD  = $(LIB_hcrypto) $(LIB_roken) $(top_builddir)/lib/otp/libotp.la
otpprint_SOURCES = otpprint.c otp_locl.h

otpprint_LDADD  = $(LIB_hcrypto) $(LIB_roken) $(top_builddir)/lib/otp/libotp.la

man_MANS = otp.1  otpprint.1

EXTRA_DIST = NTMakefile $(man_MANS)
