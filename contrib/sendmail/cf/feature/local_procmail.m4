divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1994 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`@(#)local_procmail.m4	8.11 (Berkeley) 5/19/1998')
divert(-1)

define(`LOCAL_MAILER_PATH',
	ifelse(_ARG_, `',
		ifdef(`PROCMAIL_MAILER_PATH',
			PROCMAIL_MAILER_PATH,
			`/usr/local/bin/procmail'),
		_ARG_))
define(`LOCAL_MAILER_FLAGS', `SPfhn9')
define(`LOCAL_MAILER_ARGS', `procmail -Y -a $h -d $u')
