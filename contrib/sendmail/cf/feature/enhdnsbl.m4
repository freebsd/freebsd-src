divert(-1)
#
# Copyright (c) 2000-2002, 2005, 2006 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

ifelse(defn(`_ARG_'), `', 
	`errprint(`*** ERROR: missing argument for FEATURE(`enhdnsbl')')')
divert(0)
ifdef(`_EDNSBL_R_',`dnl',`dnl
VERSIONID(`$Id: enhdnsbl.m4,v 1.11 2006/03/31 19:56:16 ca Exp $')
LOCAL_CONFIG
define(`_EDNSBL_R_',`')dnl
# map for enhanced DNS based blacklist lookups
Kednsbl dns -R A -a. -T<TMP> -r`'ifdef(`EDNSBL_TO',`EDNSBL_TO',`5')
')
divert(-1)
define(`_EDNSBL_SRV_', `_ARG_')dnl
define(`_EDNSBL_MSG_', `ifelse(len(X`'_ARG2_),`1',`"550 Rejected: " $`'&{client_addr} " listed at '_EDNSBL_SRV_`"',`_ARG2_')')dnl
define(`_EDNSBL_MSG_TMP_', `ifelse(_ARG3_,`t',`"451 Temporary lookup failure of " $`'&{client_addr} " at '_EDNSBL_SRV_`"',`_ARG3_')')dnl
define(`_EDNSBL_MATCH_', `ifelse(len(X`'_ARG4_),`1',`$`'+',_ARG4_)')dnl
divert(8)
# DNS based IP address spam list _EDNSBL_SRV_
R$*			$: $&{client_addr}
R$-.$-.$-.$-		$: <?> $(ednsbl $4.$3.$2.$1._EDNSBL_SRV_. $: OK $)
R<?>OK			$: OKSOFAR
ifelse(len(X`'_ARG3_),`1',
`R<?>$+<TMP>		$: TMPOK',
`R<?>$+<TMP>		$#error $@ 4.4.3 $: _EDNSBL_MSG_TMP_')
R<?>_EDNSBL_MATCH_	$#error $@ 5.7.1 $: _EDNSBL_MSG_
ifelse(len(X`'_ARG5_),`1',`dnl',
`R<?>_ARG5_	$#error $@ 5.7.1 $: _EDNSBL_MSG_')
ifelse(len(X`'_ARG6_),`1',`dnl',
`R<?>_ARG6_	$#error $@ 5.7.1 $: _EDNSBL_MSG_')
ifelse(len(X`'_ARG7_),`1',`dnl',
`R<?>_ARG7_	$#error $@ 5.7.1 $: _EDNSBL_MSG_')
ifelse(len(X`'_ARG8_),`1',`dnl',
`R<?>_ARG8_	$#error $@ 5.7.1 $: _EDNSBL_MSG_')
ifelse(len(X`'_ARG9_),`1',`dnl',
`R<?>_ARG9_	$#error $@ 5.7.1 $: _EDNSBL_MSG_')
divert(-1)
