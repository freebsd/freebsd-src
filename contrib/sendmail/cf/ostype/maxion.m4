divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1996 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#	Concurrent Computer Corporation Maxion system support contributed
#	by Donald R. Laster Jr. <Laster@access.digex.com>.
#

divert(0)
VERSIONID(`@(#)maxion.m4	8.11 (Berkeley) 10/6/1998')

define(`ALIAS_FILE',        ifdef(`_USE_ETC_MAIL_', `/etc/mail/aliases', `/etc/ucbmail/aliases'))dnl
define(`HELP_FILE',         ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/etc/ucbmail/sendmail.hf'))dnl
define(`QUEUE_DIR',         `/var/spool/mqueue')dnl
define(`STATUS_FILE',       ifdef(`_USE_ETC_MAIL_', `/etc/mail/statistics', `/var/adm/log/sendmail.st'))dnl
define(`LOCAL_MAILER_PATH', `/usr/bin/mail')dnl
define(`LOCAL_MAILER_FLAGS',`rmn9')dnl
define(`LOCAL_SHELL_FLAGS', `ehuP')dnl
define(`LOCAL_MAILER_ARGS', `mail $u')dnl
define(`UUCP_MAILER_ARGS',  `uux - -r -a$g -gmedium $h!rmail ($u)')dnl
define(`confEBINDIR',	    `/usr/ucblib')dnl
divert(-1)
