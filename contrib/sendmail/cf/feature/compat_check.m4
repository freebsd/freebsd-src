divert(-1)
#
# Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
divert(0)
VERSIONID(`$Id: compat_check.m4,v 1.3 2001/11/21 18:40:06 ca Exp $')
divert(-1)
ifdef(`_ACCESS_TABLE_', `',
`errprint(`FEATURE(`compat_check') requires FEATURE(`access_db')
')')

LOCAL_RULESETS
Scheck_compat
# look up the pair of addresses
# (we use <@> as the separator.  Note this in the map too!)
R< $+ > $| $+		$: $1 $| $2
R$+ $| < $+ >		$: $1 $| $2
R$+ $| $+		$: <$(access Compat:$1<@>$2 $:OK $)>
R$* $| $*		$@ ok
# act on the result,
# it must be one of the following... anything else will be allowed..
dnl for consistency with the other two even though discard does not take an
dnl reply code
R< DISCARD:$* >	$#discard $: $1 " - discarded by check_compat"
R< DISCARD $* >	$#discard $: $1 " - discarded by check_compat"
R< TEMP:$* >	$#error $@ TEMPFAIL $: $1 " error from check_compat. Try again later"
R< ERROR:$* >	$#error $@ UNAVAILABLE $: $1 " error from check_compat"
