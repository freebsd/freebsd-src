divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
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
VERSIONID(`$Id: local_procmail.m4,v 8.21 1999/11/18 05:06:23 ca Exp $')
divert(-1)

ifdef(`_MAILER_local_',
	`errprint(`*** FEATURE(local_procmail) must occur before MAILER(local)
')')dnl

define(`LOCAL_MAILER_PATH',
	ifelse(defn(`_ARG_'), `',
		ifdef(`PROCMAIL_MAILER_PATH',
			PROCMAIL_MAILER_PATH,
			`/usr/local/bin/procmail'),
		_ARG_))
define(`LOCAL_MAILER_ARGS',
	ifelse(len(X`'_ARG2_), `1', `procmail -Y -a $h -d $u', _ARG2_))
define(`LOCAL_MAILER_FLAGS',
	ifelse(len(X`'_ARG3_), `1', `SPfhn9', _ARG3_))
