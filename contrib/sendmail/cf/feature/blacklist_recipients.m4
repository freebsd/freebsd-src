divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`@(#)blacklist_recipients.m4	8.9 (Berkeley) 5/19/98')
divert(-1)

ifdef(`ACCESS_TABLE',
	`define(`_BLACKLIST_RCPT_', 1)',
	`errprint(`*** ERROR: FEATURE(blacklist_recipients) requires FEATURE(access_db)
')')
