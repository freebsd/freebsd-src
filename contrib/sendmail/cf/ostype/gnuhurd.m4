divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
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
VERSIONID(`@(#)gnuhurd.m4	8.7 (Berkeley) 5/19/98')
ifdef(`HELP_FILE',, `define(`HELP_FILE', /share/misc/sendmail.hf)')dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /var/log/sendmail.st)')dnl
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /libexec/mail.local)')dnl
define(`confEBINDIR', `/libexec')dnl
