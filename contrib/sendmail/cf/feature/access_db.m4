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
VERSIONID(`@(#)access_db.m4	8.8 (Berkeley) 5/19/1998')
divert(-1)

define(`ACCESS_TABLE',
	ifelse(_ARG_, `',
		DATABASE_MAP_TYPE` -o /etc/mail/access',
		`_ARG_'))dnl
