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
VERSIONID(`@(#)osf1.m4	8.10 (Berkeley) 5/19/98')
define(`ALIAS_FILE', /usr/adm/sendmail/aliases)dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /usr/adm/sendmail/sendmail.st)')dnl
ifdef(`HELP_FILE',, `define(`HELP_FILE', /usr/share/lib/sendmail.hf)')dnl
define(`confDEF_USER_ID', `daemon')
