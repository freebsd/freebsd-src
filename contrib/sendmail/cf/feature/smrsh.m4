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
VERSIONID(`@(#)smrsh.m4	8.8 (Berkeley) 5/19/1998')
divert(-1)

ifdef(`_MAILER_local_',
	`errprint(`*** FEATURE(smrsh) must occur before MAILER(local)')')dnl
define(`LOCAL_SHELL_PATH',
	ifelse(_ARG_, `',
		ifdef(`confEBINDIR', confEBINDIR, `/usr/libexec')`/smrsh',
		_ARG_))
