divert(-1)
#
# Copyright (c) 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: string.m4,v 8.2 1999/05/13 16:16:33 gshapiro Exp $
#
divert(0)dnl
define(`bldRINDEX',
`ifelse(index($1, $2), `-1', `-1', `eval(index($1, $2) + bldRINDEX(substr($1, eval(index($1, $2) + 1)), $2) + 1)')'dnl
)dnl
