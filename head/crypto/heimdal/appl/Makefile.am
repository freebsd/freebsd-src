# $Id: Makefile.am 17775 2006-06-30 20:26:15Z lha $

include $(top_srcdir)/Makefile.am.common

if OTP
dir_otp = otp
endif
if DCE
dir_dce = dceutils
endif
SUBDIRS = 					\
	  afsutil				\
	  ftp					\
	  login					\
	  $(dir_otp)				\
	  gssmask				\
	  popper				\
	  push					\
	  rsh					\
	  rcp					\
	  su					\
	  xnlock				\
	  telnet				\
	  test					\
	  kx					\
	  kf					\
	  $(dir_dce)
