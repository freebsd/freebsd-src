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
VERSIONID(`@(#)riscos4.5.m4	8.10 (Berkeley) 5/19/98')

ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `rmail -d $u')')dnl
define(`ALIAS_FILE', `/usr/lib/aliases')dnl
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', `/usr/spool/mqueue')')dnl
ifdef(`HELP_FILE',, `define(`HELP_FILE', `/usr/lib/sendmail.hf')')dnl
define(`confEBINDIR', `/usr/lib')dnl
