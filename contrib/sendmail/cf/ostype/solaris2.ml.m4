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
#	This ostype file is suitable for use on Solaris 2.x systems that
#	have mail.local installed.  It is my understanding that this is
#	standard as of Solaris 2.5.
#

divert(0)
VERSIONID(`@(#)solaris2.ml.m4	8.9 (Berkeley) 10/6/1998')
divert(-1)

define(`ALIAS_FILE', /etc/mail/aliases)
ifdef(`HELP_FILE',, `define(`HELP_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/etc/mail/sendmail.hf'))')
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/statistics', `/etc/mail/sendmail.st'))')
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', `/usr/lib/mail.local')')
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `fSmn9')')
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail.local -d $u')')
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -a$g $h!rmail ($u)')')
define(`confCW_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/local-host-names', `/etc/mail/sendmail.cw'))
define(`confEBINDIR', `/usr/lib')dnl
