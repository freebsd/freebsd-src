divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: blacklist_recipients.m4,v 1.1.1.3 2000/08/12 21:55:37 gshapiro Exp $')
divert(-1)

ifdef(`_ACCESS_TABLE_',
	`define(`_BLACKLIST_RCPT_', 1)',
	`errprint(`*** ERROR: FEATURE(blacklist_recipients) requires FEATURE(access_db)
')')
