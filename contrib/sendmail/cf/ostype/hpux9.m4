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
VERSIONID(`@(#)hpux9.m4	8.18 (Berkeley) 5/19/98')

ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /usr/spool/mqueue)')dnl
define(`ALIAS_FILE', /usr/lib/aliases)dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /usr/lib/sendmail.st)')dnl
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', `/bin/rmail')')dnl
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `m9')')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `rmail -d $u')')dnl
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gC $h!rmail ($u)')')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
define(`confEBINDIR', `/usr/lib')dnl
dnl
dnl	For maximum compability with HP-UX, use:
dnl	define(`confME_TOO', True)dnl
