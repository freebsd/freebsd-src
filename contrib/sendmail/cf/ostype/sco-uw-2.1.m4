divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  SCO UnixWare 2.1.2 ostype file
#
#	Contributed by Christopher Durham <chrisdu@SCO.COM> of SCO.
#
divert(0)
VERSIONID(`@(#)sco-uw-2.1.m4	8.7 (Berkeley) 10/6/1998')

define(`ALIAS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/aliases', `/usr/lib/mail/aliases'))dnl
ifdef(`HELP_FILE',,`define(`HELP_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/usr/ucblib/sendmail.hf'))')dnl
ifdef(`STATUS_FILE',,`define(`STATUS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/statistics', `/usr/ucblib/sendmail.st'))')dnl
define(`LOCAL_MAILER_PATH', `/usr/bin/rmail')dnl
define(`LOCAL_MAILER_FLAGS', `fhCEn9')dnl
define(`LOCAL_SHELL_FLAGS', `ehuP')dnl
define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gmedium $h!rmail ($u)')dnl
define(`LOCAL_MAILER_ARGS',`rmail $u')dnl
define(`confEBINDIR', `/usr/lib')dnl
