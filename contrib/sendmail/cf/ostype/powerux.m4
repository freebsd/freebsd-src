divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`@(#)powerux.m4	8.8 (Berkeley) 10/6/1998')

define(`ALIAS_FILE', /etc/mail/aliases)dnl
ifdef(`HELP_FILE',,`define(`HELP_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/etc/mail/sendmail.hf'))')dnl
ifdef(`STATUS_FILE',,`define(`STATUS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/statistics', `/etc/mail/sendmail.st'))')dnl
define(`LOCAL_MAILER_PATH', `/usr/bin/rmail')dnl
define(`LOCAL_MAILER_FLAGS', `mn9')dnl
define(`LOCAL_MAILER_ARGS', `rmail $u')dnl
define(`LOCAL_SHELL_FLAGS', `ehuP')dnl
define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gmedium $h!rmail ($u)')dnl
define(`confEBINDIR', `/usr/local/lib')dnl
