divert(-1)
#
# Copyright (c) 1983 Eric P. Allman
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#
# This file is the configuration for hub.freebsd.org, the project's mail
# server.  It needs to handle some ugly mail volumes.
#

divert(0)dnl
include(../m4/cf.m4)
VERSIONID(`$Id: hub.mc,v 1.1.2.5 1998/01/20 01:44:51 jmb Exp $')

OSTYPE(bsd4.4)dnl
DOMAIN(generic)dnl
MAILER(local)dnl
MAILER(smtp)dnl
MASQUERADE_AS(FreeBSD.ORG)dnl
FEATURE(mailertable, `hash -o /etc/mailertable')dnl
FEATURE(masquerade_envelope)dnl
EXPOSED_USER(root)dnl
EXPOSED_USER(mailman)dnl
define(`ALIAS_FILE', `/etc/aliases,/etc/majordomo.aliases')dnl
define(`UUCP_RELAY', uunet.uu.net)dnl
define(`BITNET_RELAY', mailhost.Berkeley.EDU)dnl
define(`CSNET_RELAY', mailhost.Berkeley.EDU)dnl
define(`confCW_FILE', `/etc/sendmail.cw')dnl
define(`confCHECKPOINT_INTERVAL', `10')dnl
define(`confAUTO_REBUILD', `True')dnl
define(`confMIN_FREE_BLOCKS', `1024')dnl
define(`confSMTP_MAILER', `smtp8')dnl
define(`confME_TOO', `True')dnl
define(`confMCI_CACHE_SIZE', `10')dnl
define(`confMCI_CACHE_TIMEOUT', `1h')dnl
define(`confTO_QUEUEWARN', `1d')dnl
define(`confTO_QUEUEWARN_NORMAL', `1d')dnl
define(`confTO_INITIAL', `1m')dnl
define(`confTO_CONNECT', `1m')dnl
define(`confTO_ICONNECT', `30s')dnl
define(`confTO_HELO', `2m')dnl
define(`confTO_MAIL', `4m')dnl
define(`confTO_RCPT', `4m')dnl
define(`confTO_DATAINIT', `2m')dnl
define(`confTO_DATABLOCK', `10m')dnl
define(`confTO_DATAFINAL', `10m')dnl
define(`confTO_RSET', `1m')dnl
define(`confTO_COMMAND', `5m')dnl
define(`confTO_HOSTSTATUS', `30m')dnl
define(`confMIN_QUEUE_AGE', `10m')dnl
define(`confMAX_QUEUE_RUN_SIZE', `10000')dnl
define(`confQUEUE_SORT_ORDER', `host')dnl
define(`confNO_RCPT_ACTION', `add-to-undisclosed')dnl
define(`confTRUSTED_USERS', `majordom')dnl
define(`confRECEIVED_HEADER', `$?sfrom $s $.$?_($?s$|from $.$_)
          $.by $j ($v/$Z)$?r with $r$. id $i$?u
          for $u; $|;
          $.$b$?g
          (envelope-from $g)$.')dnl
define(`confHOST_STATUS_DIRECTORY', `/var/spool/.hoststat')dnl
define(`confMAX_DAEMON_CHILDREN', `40')dnl
define(`confCONNECTION_THROTTLE_RATE', `1')dnl
define(`confFORWARD_PATH', `/var/forward/$u')dnl

LOCAL_CONFIG
Cw localhost freefall.freebsd.org

Kdenyip hash -o -a.REJECT /etc/mail/denyip.db
Kfakenames hash -o -a.REJECT /etc/mail/fakenames.db
Kspamsites hash -o -a.REJECT /etc/mail/spamsites.db

# helper rulsesets; useful for debugging sendmail configurations
#
#
Scheck_rbl
# lookup up an ip address in the Realtime Blackhole List.
R$-.$-.$-.$-	$: $(host $4.$3.$2.$1.rbl.maps.vix.com. $:OK $)

Sxlat						# for sendmail -bt
# sendmail treats "$" and "|" as two distinct tokens
# this rule "pastes" them together into one token
# and then calls check_relay.
R$* $$| $*		$: $1 $| $2
R$* $| $*		$@ $>check_relay $1 $| $2

Scheck_relay
# called with "hostname.tld $| IP address" of connecting host.
# hostname.tld is the fully-qualified domain name
# IP address is dotted-quad with surrounding "[]" brackets.
#
# each group of rules in this ruleset is independent.
# each accepts and return "hostname.tld $| IP address"
# use the ones that you want comment out the rest
# you may rearrange the groups but not the rules in each group.
# each group is preceded and followed by a comment
#
# host must NOT be in the "spamsites" database--BEGIN
R$* $| $*		$: <$1 $| $2> $1
R<$*> $+.$+.$+		<$1> $3.$4
R<$*> $+.$+		$: <$1> $(spamsites $2.$3 $)
R<$*> $*.REJECT		$#error $: "521 blocked. contact postmaster@FreeBSD.ORG"
R<$*> $*		$: $1
# host must NOT be in the "spamsites" database--END
# ip address must NOT be in the "denyip" database--BEGIN
R$* $| $*		$: $1 $| $(denyip $2 $)
R$* $| $*.REJECT	$#error $: "521 blocked. contact postmaster@FreeBSD.ORG"
# ip address must NOT be in the "denyip" database--END
# ip address must NOT be in Paul Vixie's RBL--BEGIN
R$* $| $*		$: <$1 $| $2> $>check_rbl $2
R$*.com.		$#error $: "550 Mail refused, see http://maps.vix.com/rbl"
R<$*> $*		$: $1
# ip address must NOT be in Paul Vixie's RBL--END
R$*			$@ OK

Scheck_mail
# called with envelope sender (everything after ":") in
# "Mail From: xxx", of SMTP conversation
#	may or may not have "<" ">"
# the groups of rules in this ruleset ARE NOT independent.
# "remove all RFC-822 comments" must come first
# "Connecting Host" and "Paul Vixie's RBL" must be last
#
# use the ones that you want comment out the rest
# each group is preceded and followed by a comment
#
# remove all RFC-822 comments--BEGIN
# MUST be first rule in check_mail rulseset.
R$*			$: $>3 $1
# remove all RFC-822 comments--END
# mail must come from a DNS resolvable host--BEGIN
R$* < @ $+ . >		$: $1 @ $2
R$* < @ $+ >		$#error $: "451 Domain does not resolve"
# mail must come from a DNS resolvable host--END
# mail must NOT come from a known source of spam--BEGIN
# resolved.  second check:  one of the know spam sources?
R$+ @$+			$: <$1@$2> $2
R<$*> $+.$+.$+		<$1> $3.$4
R<$*> $*		$: $(spamsites $2 $: OK $)
R$+.REJECT		$#error $: 521 $1 
R<$*> $*		$: $1
# mail must NOT come from a known source of spam--END
# Connecting Host must resolve--BEGIN
R$*			$: $1 $: $(dequote "" $&{client_name} $)
R$*			$: $>3 foo@$1
R<$*> $*<@$*>		$#error $: "451 Domain does not resolve"
# Connecting Host must resolve--END
R$*			$@ OK

Scheck_rcpt
# called with envelope recipient (everything after ":") in
# "Rcpt To: xxx", of SMTP conversation
#       may or may not have "<" ">" and or RFC-822 comments.
#	let ruleset 3 clean this up for us.
# mail must NOT be addressed "fakenames"--BEGIN
R$*			$: <$1> $>3 $1
R<$*> $+ < @ $+ >	$: <$1> $(fakenames $2 $: OK $)
R$+.REJECT		$#error $: 521 $1
R<$*> $*		$: $1
# mail must NOT be addressed "fakenames"--END
# mail must come from or go to this mahcine or machines we allow to relay--BEGIN
# R$*			$: $>Parse0 $>3 $1
# R$+ < @ $* . > $*	$: $1 < @ $2 >
# R<$+ @ $=w>		$@ OK
# R<$+ @ $* $=R>		$@ OK
# R$*			$: $(dequote "" $&{client_name} $)
# R$=w			$@ OK
# R$* $=R			$@ OK
# R$@			$@ OK
# R$*			$#error $: "550 Relaying Denied"
# mail must come from or go to this mahcine or machines we allow to relay--BEGIN
R$*			$@ OK

