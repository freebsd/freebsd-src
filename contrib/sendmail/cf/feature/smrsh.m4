divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
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
VERSIONID(`$Id: smrsh.m4,v 8.14 1999/11/18 05:06:23 ca Exp $')
divert(-1)

ifdef(`_MAILER_local_',
	`errprint(`*** FEATURE(smrsh) must occur before MAILER(local)
')')dnl
define(`LOCAL_SHELL_PATH',
	ifelse(defn(`_ARG_'), `',
		ifdef(`confEBINDIR', confEBINDIR, `/usr/libexec')`/smrsh',
		_ARG_))
_DEFIFNOT(`LOCAL_SHELL_ARGS', `smrsh -c $u')
