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

divert(0)
VERSIONID(`@(#)sinix.m4	8.8 (Berkeley) 5/19/98')
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /var/spool/mqueue)')dnl
define(`ALIAS_FILE', /etc/aliases)dnl
define(`LOCAL_MAILER_PATH', `/bin/mail.local')dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /var/sendmail.st)')dnl
ifdef(`HELP_FILE',, `define(`HELP_FILE', /etc/sendmail.hf)')dnl
define(`confEBINDIR', `/usr/ucblib')dnl
