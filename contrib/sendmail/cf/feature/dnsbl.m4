divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
ifdef(`_DNSBL_R_',`dnl',`dnl
VERSIONID(`$Id: dnsbl.m4,v 8.18 1999/08/03 04:30:56 gshapiro Exp $')')
divert(-1)
define(`_DNSBL_SRV_', `ifelse(len(X`'_ARG_),`1',`rbl.maps.vix.com',_ARG_)')dnl
define(`_DNSBL_MSG_', `ifelse(len(X`'_ARG2_),`1',`"550 Mail from " $`'&{client_addr} " refused by blackhole site '_DNSBL_SRV_`"',`_ARG2_')')dnl
divert(8)
# DNS based IP address spam list _DNSBL_SRV_
R$*			$: $&{client_addr}
R::ffff:$-.$-.$-.$-	$: <?> $(host $4.$3.$2.$1._DNSBL_SRV_. $: OK $)
R$-.$-.$-.$-		$: <?> $(host $4.$3.$2.$1._DNSBL_SRV_. $: OK $)
R<?>OK			$: OKSOFAR
R<?>$+			$#error $@ 5.7.1 $: _DNSBL_MSG_
divert(-1)
