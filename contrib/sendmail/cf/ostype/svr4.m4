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
VERSIONID(`@(#)svr4.m4	8.11 (Berkeley) 10/6/1998')

define(`ALIAS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/aliases', `/usr/ucblib/aliases'))dnl
ifdef(`HELP_FILE',,`define(`HELP_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/usr/ucblib/sendmail.hf'))')dnl
ifdef(`STATUS_FILE',,`define(`STATUS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/statistics', `/usr/ucblib/sendmail.st'))')dnl
define(`LOCAL_MAILER_PATH', `/usr/ucblib/binmail')dnl
define(`LOCAL_MAILER_FLAGS', `rmn9')dnl
define(`LOCAL_SHELL_FLAGS', `ehuP')dnl
define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gmedium $h!rmail ($u)')dnl
define(`confEBINDIR', `/usr/ucblib')dnl
