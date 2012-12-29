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
#	$Id: list.m4,v 8.4 2012/01/21 00:07:07 ashish Exp $
#
divert(0)dnl
define(`bldLIST_PUSH_ITEM',
`define(`$1', ifdef(`$1', `$1 $2 ', `$2 '))'
)dnl
define(`bldFOREACH',
`$1substr($2, `0', index($2, ` ')))`'ifelse(index($2, ` '), eval(len($2)-1),  , `bldFOREACH(`$1', substr($2, index($2, ` ')))')'dnl
)dnl

define(`bldADD_PATH', `$1/$2 ')dnl
define(`bldADD_PATHS',
`bldFOREACH(`bldADD_PATH(`$1',', $2)'dnl
)dnl
