# $Id: Makefile,v 1.2 2000/11/19 23:54:04 agmorgan Exp $
#
# This Makefile controls a build process of $(TITLE) module for
# Linux-PAM. You should not modify this Makefile (unless you know
# what you are doing!).
#

include ../../Make.Rules

TITLE=pam_group
LOCAL_CONFILE=./group.conf
INSTALLED_CONFILE=$(SCONFIGD)/group.conf

DEFS=-DDEFAULT_CONF_FILE=\"$(CONFILE)\"
CFLAGS += $(DEFS)

MODULE_SIMPLE_INSTALL=bash -f ../install_conf "$(FAKEROOT)" "$(SCONFIGD)" "$(INSTALLED_CONFILE)" "$(TITLE)" "$(LOCAL_CONFILE)"
MODULE_SIMPLE_REMOVE=rm -f $(FAKEROOT)$(INSTALLED_CONFILE)
MODULE_SIMPLE_CLEAN=rm -f ./.ignore_age

include ../Simple.Rules
