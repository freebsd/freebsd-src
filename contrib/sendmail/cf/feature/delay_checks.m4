divert(-1)
#
# Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: delay_checks.m4,v 8.7 2000/02/26 01:32:02 gshapiro Exp $')
divert(-1)

define(`_DELAY_CHECKS_', 1)
ifelse(defn(`_ARG_'), `',  `',
	lower(substr(_ARG_,0,1)), `f', `define(`_SPAM_FRIEND_', 1) define(`_SPAM_FH_', 1)',
	lower(substr(_ARG_,0,1)), `h', `define(`_SPAM_HATER_', 1)  define(`_SPAM_FH_', 1)',
	`errprint(`*** ERROR: illegal argument _ARG_ for FEATURE(delay_checks)
')
	')
