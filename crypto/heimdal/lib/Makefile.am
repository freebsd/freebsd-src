# $Id: Makefile.am,v 1.21 2000/11/15 23:11:05 assar Exp $

include $(top_srcdir)/Makefile.am.common

if KRB4
dir_45 = 45
endif
if OTP
dir_otp = otp
endif
if DCE
dir_dce = kdfs
endif

SUBDIRS = @DIR_roken@ vers editline com_err sl asn1 @DIR_des@ krb5 \
	kafs hdb kadm5 gssapi auth $(dir_45) $(dir_otp) $(dir_dce)
