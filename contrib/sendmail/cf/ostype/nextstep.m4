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
VERSIONID(`@(#)nextstep.m4	8.14 (Berkeley) 5/19/98')
define(`ALIAS_FILE', /etc/sendmail/aliases)dnl
define(`confCW_FILE', /etc/sendmail/sendmail.cw)dnl
ifdef(`HELP_FILE',, `define(`HELP_FILE', /usr/lib/sendmail.hf)')dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /etc/sendmail/sendmail.st)')dnl
ifdef(`UUCP_MAILER_PATH',, `define(`UUCP_MAILER_PATH', /usr/bin/uux)')dnl
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /usr/spool/mqueue)')dnl
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `rmnP9')')dnl
ifdef(`LOCAL_SHELL_FLAGS',, `define(`LOCAL_SHELL_FLAGS', `euP')')dnl
define(`confEBINDIR', `/usr/lib')dnl
