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
VERSIONID(`@(#)local_lmtp.m4	8.5 (Berkeley) 5/19/98')
divert(-1)

define(`LOCAL_MAILER_PATH',
	ifelse(_ARG_, `',
		ifdef(`confEBINDIR', confEBINDIR, `/usr/libexec')`/mail.local',
		_ARG_))
define(`LOCAL_MAILER_FLAGS', `SXfmnz9')
define(`LOCAL_MAILER_ARGS', `mail.local -l')
