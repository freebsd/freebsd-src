divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1997 Eric P. Allman.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#	Contributed by Glen McCready <glen@qnx.com>
#

divert(0)
VERSIONID(`@(#)qnx.m4	8.8 (Berkeley) 10/6/1998')
define(`QUEUE_DIR', /usr/spool/mqueue)dnl
define(`HELP_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/etc/sendmail.hf'))dnl
define(`LOCAL_MAILER_ARGS', `mail $u')dnl
define(`LOCAL_MAILER_FLAGS', `Sh')dnl
define(`LOCAL_MAILER_PATH', /usr/bin/mailx)dnl
define(`UUCP_MAILER_ARGS', `uux - -r -z -a$f $h!rmail ($u)')dnl
