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
VERSIONID(`@(#)mailertable.m4	8.10 (Berkeley) 10/6/1998')
divert(-1)

define(`MAILER_TABLE', ifelse(_ARG_, `',
			      ifdef(`_USE_ETC_MAIL_',
				    DATABASE_MAP_TYPE` -o /etc/mail/mailertable',
				    DATABASE_MAP_TYPE` -o /etc/mailertable'),
			      `_ARG_'))dnl
