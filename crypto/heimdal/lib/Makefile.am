# $Id: Makefile.am,v 1.16 1999/04/01 15:03:37 joda Exp $

include $(top_srcdir)/Makefile.am.common

if KRB4
dir_45 = 45
endif
if OTP
dir_otp = otp
endif

SUBDIRS = roken editline com_err sl asn1 des krb5 \
	kafs hdb kadm5 gssapi auth $(dir_45) $(dir_otp)
