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
divert(0)

VERSIONID(`@(#)proto.m4	8.45 (Berkeley) 3/4/94')

MAILER(local)dnl

ifdef(`_OLD_SENDMAIL_',
`define(`_SET_95_', 5)dnl
define(`_SET_96_', 6)dnl
define(`_SET_97_', 7)dnl
define(`_SET_98_', 8)dnl
define(`confDOMAIN_NAME',
	`ifdef(`NEED_DOMAIN', `$w.$d', `$w')')dnl',
`# level 5 config file format
V5
define(`_SET_95_', 95)dnl
define(`_SET_96_', 96)dnl
define(`_SET_97_', 97)dnl
define(`_SET_98_', 98)dnl')
ifdef(`confSMTP_MAILER',, `define(`confSMTP_MAILER', `smtp')')dnl
ifdef(`confLOCAL_MAILER',, `define(`confLOCAL_MAILER', `local')')dnl
ifdef(`confRELAY_MAILER',,
	`define(`confRELAY_MAILER',
		`ifdef(`_MAILER_smtp_', `relay',
			`ifdef(`_MAILER_uucp', `suucp', `unknown')')')')dnl
define(`_SMTP_', `confSMTP_MAILER')dnl		for readability only
define(`_LOCAL_', `confLOCAL_MAILER')dnl	for readability only
define(`_RELAY_', `confRELAY_MAILER')dnl	for readability only

##################
#   local info   #
##################

Cwlocalhost
ifdef(`USE_CW_FILE',
`# file containing names of hosts for which we receive email
Fw`'confCW_FILE',
	`dnl')
ifdef(`confDOMAIN_NAME', `
# my official domain name
Dj`'confDOMAIN_NAME',
	`dnl')

ifdef(`_NULL_CLIENT_ONLY_',
`include(../m4/nullrelay.m4)m4exit',
	`dnl')

CP.

ifdef(`UUCP_RELAY',
`# UUCP relay host
DY`'UUCP_RELAY
CPUUCP

')dnl
ifdef(`BITNET_RELAY',
`#  BITNET relay host
DB`'BITNET_RELAY
CPBITNET

')dnl
ifdef(`FAX_RELAY',
`# FAX relay host
DF`'FAX_RELAY
CPFAX

')dnl
# "Smart" relay host (may be null)
DS`'ifdef(`SMART_HOST', SMART_HOST)

ifdef(`MAILER_TABLE',
`# Mailer table (overriding domains)
Kmailertable MAILER_TABLE

')dnl
ifdef(`DOMAIN_TABLE',
`# Domain table (adding domains)
Kdomaintable DOMAIN_TABLE

')dnl
# who I send unqualified names to (null means deliver locally)
DR`'ifdef(`LOCAL_RELAY', LOCAL_RELAY)

# who gets all local email traffic ($R has precedence for unqualified names)
DH`'ifdef(`MAIL_HUB', MAIL_HUB)

# who I masquerade as (null for no masquerading)
DM`'ifdef(`MASQUERADE_NAME', MASQUERADE_NAME)

# class L: names that should be delivered locally, even if we have a relay
# class E: names that should be exposed as from this host, even if we masquerade
#CLroot
CEroot
undivert(5)dnl

# operators that cannot be in local usernames (i.e., network indicators)
CO @ % ifdef(`_NO_UUCP_', `', `!')

# a class with just dot (for identifying canonical names)
C..

ifdef(`_OLD_SENDMAIL_', `dnl',
`# dequoting map
Kdequote dequote')

undivert(6)dnl

######################
#   Special macros   #
######################

# SMTP initial login message
De`'confSMTP_LOGIN_MSG

# UNIX initial From header format
Dl`'confFROM_LINE

# my name for error messages
Dn`'confMAILER_NAME

# delimiter (operator) characters
Do`'confOPERATORS

# format of a total name
Dq`'ifdef(`confFROM_HEADER', confFROM_HEADER,
	ifdef(`_OLD_SENDMAIL_', `$g$?x ($x)$.', `$?x$x <$g>$|$g$.'))
include(`../m4/version.m4')

###############
#   Options   #
###############

# strip message body to 7 bits on input?
O7`'confSEVEN_BIT_INPUT

# wait (in minutes) for alias file rebuild
Oa`'confALIAS_WAIT

# location of alias file
OA`'ifdef(`ALIAS_FILE', `ALIAS_FILE', /etc/aliases)

# minimum number of free blocks on filesystem
Ob`'confMIN_FREE_BLOCKS

# substitution for space (blank) characters
OB`'confBLANK_SUB

# avoid connecting to "expensive" mailers on initial submission?
Oc`'confCON_EXPENSIVE

# checkpoint queue runs after every N successful deliveries
OC`'confCHECKPOINT_INTERVAL

# default delivery mode
Od`'confDELIVERY_MODE

# automatically rebuild the alias database?
OD`'confAUTO_REBUILD

# error message header/file
ifdef(`confERROR_MESSAGE',
	OE`'confERROR_MESSAGE,
	#OE/etc/sendmail.oE)

# error mode
ifdef(`confERROR_MODE',
	Oe`'confERROR_MODE,
	#Oep)

# save Unix-style "From_" lines at top of header?
Of`'confSAVE_FROM_LINES

# temporary file mode
OF`'confTEMP_FILE_MODE

# match recipients against GECOS field?
OG`'confMATCH_GECOS

# default GID
Og`'confDEF_GROUP_ID

# maximum hop count
Oh`'confMAX_HOP

# location of help file
OH`'ifdef(`HELP_FILE', HELP_FILE, /usr/lib/sendmail.hf)

# ignore dots as terminators in incoming messages?
Oi`'confIGNORE_DOTS

# Insist that the BIND name server be running to resolve names
ifdef(`confBIND_OPTS',
	OI`'confBIND_OPTS,
	#OI)

# deliver MIME-encapsulated error messages?
Oj`'confMIME_FORMAT_ERRORS

# Forward file search path
ifdef(`confFORWARD_PATH',
	OJ`'confFORWARD_PATH,
	#OJ/var/forward/$u:$z/.forward.$w:$z/.forward)

# open connection cache size
Ok`'confMCI_CACHE_SIZE

# open connection cache timeout
OK`'confMCI_CACHE_TIMEOUT

# use Errors-To: header?
Ol`'confUSE_ERRORS_TO

# log level
OL`'confLOG_LEVEL

# send to me too, even in an alias expansion?
Om`'confME_TOO

# verify RHS in newaliases?
On`'confCHECK_ALIASES

# default messages to old style headers if no special punctuation?
Oo`'confOLD_STYLE_HEADERS

# SMTP daemon options
ifdef(`confDAEMON_OPTIONS',
	OO`'confDAEMON_OPTIONS,
	#OOPort=esmtp)

# privacy flags
Op`'confPRIVACY_FLAGS

# who (if anyone) should get extra copies of error messages
ifdef(`confCOPY_ERRORS_TO',
	OP`'confCOPY_ERRORS_TO,
	#OPPostmaster)

# slope of queue-only function
ifdef(`confQUEUE_FACTOR',
	Oq`'confQUEUE_FACTOR,
	#Oq600000)

# queue directory
OQ`'ifdef(`QUEUE_DIR', QUEUE_DIR, /var/spool/mqueue)

# read timeout -- now OK per RFC 1123 section 5.3.2
ifdef(`confREAD_TIMEOUT',
	Or`'confREAD_TIMEOUT,
	#Ordatablock=10m)

# queue up everything before forking?
Os`'confSAFE_QUEUE

# status file
OS`'ifdef(`STATUS_FILE', STATUS_FILE, /etc/sendmail.st)

# default message timeout interval
OT`'confMESSAGE_TIMEOUT

# time zone handling:
#  if undefined, use system default
#  if defined but null, use TZ envariable passed in
#  if defined and non-null, use that info
ifelse(confTIME_ZONE, `USE_SYSTEM', `#Ot',
	confTIME_ZONE, `USE_TZ', `Ot',
	`Ot`'confTIME_ZONE')

# default UID
Ou`'confDEF_USER_ID

# list of locations of user database file (null means no lookup)
OU`'ifdef(`confUSERDB_SPEC', `confUSERDB_SPEC')

# fallback MX host
ifdef(`confFALLBACK_MX',
	OV`'confFALLBACK_MX,
	#OVfall.back.host.net)

# if we are the best MX host for a site, try it directly instead of config err
Ow`'confTRY_NULL_MX_LIST

# load average at which we just queue messages
Ox`'confQUEUE_LA

# load average at which we refuse connections
OX`'confREFUSE_LA

# work recipient factor
ifdef(`confWORK_RECIPIENT_FACTOR',
	Oy`'confWORK_RECIPIENT_FACTOR,
	#Oy30000)

# deliver each queued job in a separate process?
OY`'confSEPARATE_PROC

# work class factor
ifdef(`confWORK_CLASS_FACTOR',
	Oz`'confWORK_CLASS_FACTOR,
	#Oz1800)

# work time factor
ifdef(`confWORK_TIME_FACTOR',
	OZ`'confWORK_TIME_FACTOR,
	#OZ90000)

###########################
#   Message precedences   #
###########################

Pfirst-class=0
Pspecial-delivery=100
Plist=-30
Pbulk=-60
Pjunk=-100

#####################
#   Trusted users   #
#####################

Troot
Tdaemon
Tuucp

#########################
#   Format of headers   #
#########################

H?P?Return-Path: $g
HReceived: $?sfrom $s $.$?_($?s$|from $.$_) $.by $j ($v/$Z)$?r with $r$. id $i$?u for $u$.; $b
H?D?Resent-Date: $a
H?D?Date: $a
H?F?Resent-From: $q
H?F?From: $q
H?x?Full-Name: $x
HSubject:
# HPosted-Date: $a
# H?l?Received-Date: $b
H?M?Resent-Message-Id: <$t.$i@$j>
H?M?Message-Id: <$t.$i@$j>
#
######################################################################
######################################################################
#####
#####			REWRITING RULES
#####
######################################################################
######################################################################

undivert(9)dnl

###########################################
###  Rulset 3 -- Name Canonicalization  ###
###########################################
S3

# handle null input (translate to <@> special case)
R$@			$@ <@>

# basic textual canonicalization -- note RFC733 heuristic here
R$*<$*>$*<$*>$*		$2$3<$4>$5			strip multiple <> <>
R$*<$*<$+>$*>$*		<$3>$5				2-level <> nesting
R$*<>$*			$@ <@>				MAIL FROM:<> case
R$*<$+>$*		$2				basic RFC821/822 parsing

# handle list:; syntax as special case
R$*:;$*			$@ $1 :; <@>

# make sure <@a,@b,@c:user@d> syntax is easy to parse -- undone later
R@ $+ , $+		@ $1 : $2			change all "," to ":"

# localize and dispose of route-based addresses
R@ $+ : $+		$@ $>_SET_96_ < @$1 > : $2		handle <route-addr>

# find focus for list syntax
R $+ : $* ; @ $+	$@ $>_SET_96_ $1 : $2 ; < @ $3 >	list syntax
R $+ : $* ;		$@ $1 : $2;			list syntax

# find focus for @ syntax addresses
R$+ @ $+		$: $1 < @ $2 >			focus on domain
R$+ < $+ @ $+ >		$1 $2 < @ $3 >			move gaze right
R$+ < @ $+ >		$@ $>_SET_96_ $1 < @ $2 >		already canonical

# do some sanity checking
R$* < @ $* : $* > $*	$1 < @ $2 $3 > $4		nix colons in addrs

ifdef(`_NO_UUCP_', `dnl',
`# convert old-style addresses to a domain-based address
R$- ! $+		$@ $>_SET_96_ $2 < @ $1 .UUCP >	resolve uucp names
R$+ . $- ! $+		$@ $>_SET_96_ $3 < @ $1 . $2 >		domain uucps
R$+ ! $+		$@ $>_SET_96_ $2 < @ $1 .UUCP >	uucp subdomains')

# if we have % signs, take the rightmost one
R$* % $*		$1 @ $2				First make them all @s.
R$* @ $* @ $*		$1 % $2 @ $3			Undo all but the last.
R$* @ $*		$@ $>_SET_96_ $1 < @ $2 >		Insert < > and finish

# else we must be a local name


################################################
###  Ruleset _SET_96_ -- bottom half of ruleset 3  ###
################################################

#  At this point, everything should be in a "local_part<@domain>extra" format.
S`'_SET_96_

# handle special cases for local names
R$* < @ localhost > $*		$: $1 < @ $j . > $2		no domain at all
R$* < @ localhost . $m > $*	$: $1 < @ $j . > $2		local domain
ifdef(`_NO_UUCP_', `dnl',
`R$* < @ localhost . UUCP > $*	$: $1 < @ $j . > $2		.UUCP domain')
R$* < @ [ $+ ] > $*		$: $1 < @@ [ $2 ] > $3		mark [a.b.c.d]
R$* < @@ $=w > $*		$: $1 < @ $j . > $3		self-literal
R$* < @@ $+ > $*		$@ $1 < @ $2 > $3		canon IP addr
ifdef(`DOMAIN_TABLE', `
# look up unqualified domains in the domain table
R$* < @ $- > $*			$: $1 < @ $(domaintable $2 $) > $3',
`dnl')
undivert(2)dnl

ifdef(`_NO_UUCP_', `dnl',
`ifdef(`UUCP_RELAY',
`# pass UUCP addresses straight through
R$* < @ $+ . UUCP > $*		$@ $1 < @ $2 . UUCP . > $3',
`# if really UUCP, handle it immediately
ifdef(`_CLASS_U_',
`R$* < @ $=U . UUCP > $*	$@ $1 < @ $2 . UUCP . > $3', `dnl')
ifdef(`_CLASS_V_',
`R$* < @ $=V . UUCP > $*	$@ $1 < @ $2 . UUCP . > $3', `dnl')
ifdef(`_CLASS_W_',
`R$* < @ $=W . UUCP > $*	$@ $1 < @ $2 . UUCP . > $3', `dnl')
ifdef(`_CLASS_X_',
`R$* < @ $=X . UUCP > $*	$@ $1 < @ $2 . UUCP . > $3', `dnl')
ifdef(`_CLASS_Y_',
`R$* < @ $=Y . UUCP > $*	$@ $1 < @ $2 . UUCP . > $3', `dnl')

# try UUCP traffic as a local address
R$* < @ $+ . UUCP > $*		$: $1 < @ $[ $2 $] . UUCP . > $3
ifdef(`_OLD_SENDMAIL_',
`R$* < @ $+ . $+ . UUCP . > $*		$@ $1 < @ $2 . $3 . > $4',
`R$* < @ $+ . . UUCP . > $*		$@ $1 < @ $2 . > $3')')
')
ifdef(`_NO_CANONIFY_', `dnl',
`# pass to name server to make hostname canonical
R$* < @ $* $~P > $*		$: $1 < @ $[ $2 $3 $] > $4')

# local host aliases and pseudo-domains are always canonical
R$* < @ $=w > $*		$: $1 < @ $2 . > $3
R$* < @ $* $=P > $*		$: $1 < @ $2 $3 . > $4
R$* < @ $* . . > $*		$1 < @ $2 . > $3

# if this is the local hostname, make sure we treat is as canonical
R$* < @ $j > $*			$: $1 < @ $j . > $2


##################################################
###  Ruleset 4 -- Final Output Post-rewriting  ###
##################################################
S4

R$*<@>			$@ $1				handle <> and list:;

# strip trailing dot off possibly canonical name
R$* < @ $+ . > $*	$1 < @ $2 > $3

# externalize local domain info
R$* < $+ > $*		$1 $2 $3			defocus
R@ $+ : @ $+ : $+	@ $1 , @ $2 : $3		<route-addr> canonical
R@ $*			$@ @ $1				... and exit

ifdef(`_NO_UUCP_', `dnl',
`# UUCP must always be presented in old form
R$+ @ $- . UUCP		$2!$1				u@h.UUCP => h!u')

# delete duplicate local names
R$+ % $=w @ $=w		$1 @ $j				u%host@host => u@host



##############################################################
###   Ruleset _SET_97_ -- recanonicalize and call ruleset zero   ###
###		   (used for recursive calls)		   ###
##############################################################

S`'_SET_97_
R$*			$: $>3 $1
R$*			$@ $>0 $1


######################################
###   Ruleset 0 -- Parse Address   ###
######################################

S0

R<@>			$#_LOCAL_ $: <>			special case error msgs
R$* : $* ;		$#error $@ USAGE $: "list:; syntax illegal for recipient addresses"
R<@ $+>			$#error $@ USAGE $: "user address required"
R<$* : $* >		$#error $@ USAGE $: "colon illegal in host name part"

ifdef(`_MAILER_smtp_',
`# handle numeric address spec
R$* < @ [ $+ ] > $*	$: $>_SET_98_ $1 < @ [ $2 ] > $3	numeric internet spec
R$* < @ [ $+ ] > $*	$#_SMTP_ $@ [$2] $: $1 < @ [$2] > $3	still numeric: send',
	`dnl')

# now delete the local info -- note $=O to find characters that cause forwarding
R$* < @ > $*		$@ $>_SET_97_ $1		user@ => user
R< @ $=w . > : $*	$@ $>_SET_97_ $2		@here:... -> ...
R$* $=O $* < @ $=w . >	$@ $>_SET_97_ $1 $2 $3		...@here -> ...

# handle local hacks
R$*			$: $>_SET_98_ $1

# short circuit local delivery so forwarded email works
ifdef(`_LOCAL_NOT_STICKY_',
`R$=L < @ $=w . >		$#_LOCAL_ $: @ $1			special local names
R$+ < @ $=w . >		$#_LOCAL_ $: $1			dispose directly',
`R$+ < @ $=w . >		$: $1 < @ $2 . @ $H >		first try hub
ifdef(`_OLD_SENDMAIL_',
`R$+ < $+ @ $-:$+ >	$# $3 $@ $4 $: $1 < $2 >	yep ....
R$+ < $+ @ $+ >		$#relay $@ $3 $: $1 < $2 >	yep ....
R$+ < $+ @ >		$#_LOCAL_ $: $1			nope, local address',
`R$+ < $+ @ $+ >		$#_LOCAL_ $: $1			yep ....
R$+ < $+ @ >		$#_LOCAL_ $: @ $1			nope, local address')')
ifdef(`MAILER_TABLE',
`
# not local -- try mailer table lookup
R$* <@ $+ > $*		$: < $2 > $1 < @ $2 > $3	extract host name
R< $+ . > $*		$: < $1 > $2			strip trailing dot
R< $+ > $*		$: < $(mailertable $1 $) > $2	lookup
R< $- : $+ > $*		$# $1 $@ $2 $: $3		check -- resolved?
R< $+ > $*		$: $>90 <$1> $2			try domain',
`dnl')
undivert(4)dnl

ifdef(`_NO_UUCP_', `dnl',
`# resolve remotely connected UUCP links (if any)
ifdef(`_CLASS_V_',
`R$* < @ $=V . UUCP . > $*		$: $>_SET_95_ < $V > $1 <@$2.UUCP.> $3',
	`dnl')
ifdef(`_CLASS_W_',
`R$* < @ $=W . UUCP . > $*		$: $>_SET_95_ < $W > $1 <@$2.UUCP.> $3',
	`dnl')
ifdef(`_CLASS_X_',
`R$* < @ $=X . UUCP . > $*		$: $>_SET_95_ < $X > $1 <@$2.UUCP.> $3',
	`dnl')')

# resolve fake top level domains by forwarding to other hosts
ifdef(`BITNET_RELAY',
`R$*<@$+.BITNET.>$*	$: $>_SET_95_ < $B > $1 <@$2.BITNET.> $3	user@host.BITNET',
	`dnl')
ifdef(`_MAILER_pop_',
`R$+ < @ POP. >		$#pop $: $1			user@POP',
	`dnl')
ifdef(`_MAILER_fax_',
`R$+ < @ $+ .FAX. >	$#fax $@ $2 $: $1		user@host.FAX',
`ifdef(`FAX_RELAY',
`R$*<@$+.FAX.>$*		$: $>_SET_95_ < $F > $1 <@$2.FAX.> $3	user@host.FAX',
	`dnl')')

ifdef(`UUCP_RELAY',
`# forward non-local UUCP traffic to our UUCP relay
R$*<@$*.UUCP.>$*		$: $>_SET_95_ < $Y > $1 <@$2.UUCP.> $3	uucp mail',
`ifdef(`_MAILER_uucp_',
`# forward other UUCP traffic straight to UUCP
R$* < @ $+ .UUCP. > $*		$#uucp $@ $2 $: $1 < @ $2 .UUCP. > $3	user@host.UUCP',
	`dnl')')
ifdef(`_MAILER_usenet_', `
# addresses sent to net.group.USENET will get forwarded to a newsgroup
R$+ . USENET		$#usenet $: $1',
	`dnl')

ifdef(`_LOCAL_RULES_',
`# figure out what should stay in our local mail system
undivert(1)', `dnl')

# pass names that still have a host to a smarthost (if defined)
R$* < @ $* > $*		$: $>_SET_95_ < $S > $1 < @ $2 > $3	glue on smarthost name

# deal with other remote names
ifdef(`_MAILER_smtp_',
`R$* < @$* > $*		$#_SMTP_ $@ $2 $: $1 < @ $2 > $3		user@host.domain',
`R$* < @$* > $*		$#error $@NOHOST $: Unrecognized host name $2')

ifdef(`_OLD_SENDMAIL_',
`# forward remaining names to local relay, if any
R$=L			$#_LOCAL_ $: $1			special local names
R$+			$: $>_SET_95_ < $R > $1			try relay
R$+			$: $>_SET_95_ < $H > $1			try hub
R$+			$#_LOCAL_ $: $1			no relay or hub: local',

`# if this is quoted, strip the quotes and try again
R$+			$: $(dequote $1 $)		strip quotes
R$+ $=O $+		$@ $>_SET_97_ $1 $2 $3			try again

# handle locally delivered names
R$=L			$#_LOCAL_ $: @ $1			special local names
R$+			$#_LOCAL_ $: $1			regular local names

###########################################################################
###   Ruleset 5 -- special rewriting after aliases have been expanded   ###
###		   (new sendmail only)					###
###########################################################################

S5

# see if we have a relay or a hub
R$+			$: < $R > $1			try relay
R< > $+			$: < $H > $1			try hub
R< > $+			$@ $1				nope, give up
R< $- : $+ > $+		$: $>_SET_95_ < $1 : $2 > $3 < @ $2 >
R< $+ > $+		$@ $>_SET_95_ < $1 > $2 < @ $1 >')
ifdef(`MAILER_TABLE',
`

###################################################################
###  Ruleset 90 -- try domain part of mailertable entry 	###
###		   (new sendmail only)				###
###################################################################

S90
R$* <$- . $+ > $*	$: $1$2 < $(mailertable .$3 $@ $1$2 $@ $2 $) > $4
R$* <$- : $+ > $*	$# $2 $@ $3 $: $4		check -- resolved?
R$* < . $+ > $*		$@ $>90 $1 . <$2> $3		no -- strip & try again
R$* < $* > $*		$: < $(mailertable . $@ $1$2 $) > $3	try "."
R<$- : $+ > $*		$# $1 $@ $2 $: $3		"." found?
R< $* > $*		$@ $2				no mailertable match',
`dnl')

###################################################################
###  Ruleset _SET_95_ -- canonify mailer:host syntax to triple	###
###################################################################

S`'_SET_95_
R< > $*			$@ $1				strip off null relay
R< $- : $+ > $*		$# $1 $@ $2 $: $3		try qualified mailer
R< $=w > $*		$@ $2				delete local host
R< $+ > $*		$#_RELAY_ $@ $1 $: $2		use unqualified mailer

###################################################################
###  Ruleset _SET_98_ -- local part of ruleset zero (can be null)	###
###################################################################

S`'_SET_98_
undivert(3)dnl
#
######################################################################
######################################################################
#####
`#####			MAILER DEFINITIONS'
#####
######################################################################
######################################################################
undivert(7)dnl
