divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1997 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#

divert(0)
VERSIONID(`$Id: gnu.m4,v 8.13 1999/04/24 05:37:41 gshapiro Exp $')
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', `/var/log/sendmail.st')')dnl
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /libexec/mail.local)')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail $u')')dnl
define(`confEBINDIR', `/libexec')dnl
