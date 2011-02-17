# $Id: Makefile.am 20466 2007-04-20 08:29:05Z lha $

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
if COM_ERR
dir_com_err = com_err
endif
if !HAVE_OPENSSL
dir_hcrypto = hcrypto
endif

SUBDIRS = roken vers editline $(dir_com_err) sl asn1 $(dir_hcrypto) hx509 \
	krb5 ntlm kafs gssapi hdb kadm5 auth $(dir_45) $(dir_otp) $(dir_dce)
