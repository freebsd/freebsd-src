divert(-1)
#
# Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
ifdef(`_DNSBL_R_',`dnl',`dnl
VERSIONID(`$Id: dnsbl.m4,v 8.26 2001/11/12 16:04:14 ca Exp $')
define(`_DNSBL_R_',`')
LOCAL_CONFIG
# map for DNS based blacklist lookups
Kdnsbl host -T<TMP>ifdef(`DNSBL_MAP_OPT',` DNSBL_MAP_OPT')')
divert(-1)
define(`_DNSBL_SRV_', `ifelse(len(X`'_ARG_),`1',`blackholes.mail-abuse.org',_ARG_)')dnl
define(`_DNSBL_MSG_', `ifelse(len(X`'_ARG2_),`1',`"550 Mail from " $`'&{client_addr} " refused by blackhole site '_DNSBL_SRV_`"',`_ARG2_')')dnl
define(`_DNSBL_MSG_TMP_', `ifelse(_ARG3_,`t',`"451 Temporary lookup failure of " $`'&{client_addr} " at '_DNSBL_SRV_`"',`_ARG3_')')dnl
divert(8)
# DNS based IP address spam list _DNSBL_SRV_
R$*			$: $&{client_addr}
R$-.$-.$-.$-		$: <?> $(dnsbl $4.$3.$2.$1._DNSBL_SRV_. $: OK $)
R<?>OK			$: OKSOFAR
ifelse(len(X`'_ARG3_),`1',
`R<?>$+<TMP>		$: TMPOK',
`R<?>$+<TMP>		$#error $@ 4.7.1 $: _DNSBL_MSG_TMP_')
R<?>$+			$#error $@ 5.7.1 $: _DNSBL_MSG_
divert(-1)
