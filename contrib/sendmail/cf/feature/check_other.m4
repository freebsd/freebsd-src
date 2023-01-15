divert(-1)
#
# Copyright (c) 2021 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
dnl bogus Id, just here to show up in the generated cf file
divert(0)
VERSIONID(`$Id: check_other.m4,v 1.0 2021-04-30 00:01:11 ca Exp $')
divert(-1)
dnl
dnl Options:
dnl first arg:
dnl  empty: use default regex
dnl  else: use as regex
dnl  [not implemented: NOREGEX: do not use any regex]
dnl
dnl Possible enhancements:
dnl select which SMTP reply type(s) should be allowed to match?
dnl maybe add "exceptions":
dnl   - via an "OK" regex?
dnl   - access map lookup for clients to be "ok" (Connect:... {ok,relay})
dnl more args? possible matches for rejections:
dnl	does not seem to worth the effort: too inflexible.
dnl
dnl Note: sendmail removes whitespace before ':' ("tokenization")
ifelse(
   defn(`_ARG_'), `', `define(`CHKORX', `^[[:print:]]+ *:')',
   dnl defn(`_ARG_'), `NOREGEX', `define(`CHKORX', `')',
  `define(`CHKORX', defn(`_ARG_'))')
LOCAL_CONFIG
ifelse(defn(`CHKORX'), `', `', `dnl
Kbadcmd regex -m -a<BADCMD> defn(`CHKORX')')
LOCAL_RULESETS
Scheck_other
dnl accept anything that will be accepted by the MTA
R$* $| 2	$@ ok
ifelse(defn(`CHKORX'), `', `', `dnl
R$+ $| 5	$: $(badcmd $1 $)
R$*<BADCMD>	$#error $@ 4.7.0 $: 421 bad command')
dnl terminate on any bad command?
dnl R$* $| 5	$#error $@ 4.7.0 $: 421 bad command
