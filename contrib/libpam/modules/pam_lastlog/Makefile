#
# $Id: Makefile,v 1.3 2001/02/10 22:33:10 agmorgan Exp $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#
# Created by Andrew Morgan <morgan@linux.kernel.org> 2000/08/27
#

include ../../Make.Rules

ifeq ($(HAVE_LIBUTIL),yes)
 MODULE_SIMPLE_EXTRALIBS += -lutil
endif

TITLE=pam_lastlog

include ../Simple.Rules
