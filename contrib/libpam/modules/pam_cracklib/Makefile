#
# $Id: Makefile,v 1.3 2001/02/10 22:15:23 agmorgan Exp $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# Created by Andrew Morgan <morgan@kernel.org> 2000/10/08
#

include ../../Make.Rules

TITLE=pam_cracklib

ifeq ($(HAVE_LIBCRACK),yes)
BUILD_THIS_MODULE=yes
MODULE_SIMPLE_EXTRALIBS=-lcrack

# These two should really be provided by ../../pam_aconf.h
CFLAGS+=-DCRACKLIB_DICTPATH=\"$(CRACKLIB_DICTPATH)\"

ifeq ($(HAVE_LIBCRYPT),yes)
 MODULE_SIMPLE_EXTRALIBS += -lcrypt
endif

endif

ifeq ($(BUILD_THIS_MODULE),yes)
 include ../Simple.Rules
else
 include ../dont_makefile
endif
