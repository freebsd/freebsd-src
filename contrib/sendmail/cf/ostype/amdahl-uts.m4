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
VERSIONID(`@(#)amdahl-uts.m4	8.10 (Berkeley) 5/19/98')
divert(-1)

define(`ALIAS_FILE', /etc/mail/aliases)
ifdef(`HELP_FILE',, `define(`HELP_FILE', /etc/mail/sendmail.hf)')
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /usr/lib/sendmail.st)')
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `fSn9')')
define(`confCW_FILE', /etc/mail/sendmail.cw)
define(`confEBINDIR', `/usr/lib')dnl
