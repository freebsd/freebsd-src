divert(-1)
#
# Copyright (c) 1983, 1995 Eric P. Allman
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

VERSIONID(`@(#)proto.m4	8.134 (Berkeley) 10/13/96')

MAILER(local)dnl

# level 7 config file format
V7/Berkeley
divert(-1)

# do some sanity checking
ifdef(`__OSTYPE__',,
	`errprint(`*** ERROR: No system type defined (use OSTYPE macro)')')

# pick our default mailers
ifdef(`confSMTP_MAILER',, `define(`confSMTP_MAILER', `esmtp')')
ifdef(`confLOCAL_MAILER',, `define(`confLOCAL_MAILER', `local')')
ifdef(`confRELAY_MAILER',,
	`define(`confRELAY_MAILER',
		`ifdef(`_MAILER_smtp_', `relay',
			`ifdef(`_MAILER_uucp', `uucp-new', `unknown')')')')
ifdef(`confUUCP_MAILER',, `define(`confUUCP_MAILER', `uucp-old')')
define(`_SMTP_', `confSMTP_MAILER')dnl		for readability only
define(`_LOCAL_', `confLOCAL_MAILER')dnl	for readability only
define(`_RELAY_', `confRELAY_MAILER')dnl	for readability only
define(`_UUCP_', `confUUCP_MAILER')dnl		for readability only

# back compatibility with old config files
ifdef(`confDEF_GROUP_ID',
	`errprint(`*** confDEF_GROUP_ID is obsolete.')
	 errprint(`    Use confDEF_USER_ID with a colon in the value instead.')')
ifdef(`confREAD_TIMEOUT',
	`errprint(`*** confREAD_TIMEOUT is obsolete.')
	 errprint(`    Use individual confTO_<timeout> parameters instead.')')
ifdef(`confMESSAGE_TIMEOUT',
	`define(`_ARG_', index(confMESSAGE_TIMEOUT, /))
	 ifelse(_ARG_, -1,
		`define(`confTO_QUEUERETURN', confMESSAGE_TIMEOUT)',
		`define(`confTO_QUEUERETURN',
			substr(confMESSAGE_TIMEOUT, 0, _ARG_))
		 define(`confTO_QUEUEWARN',
			substr(confMESSAGE_TIMEOUT, eval(_ARG_+1)))')')
ifdef(`confMIN_FREE_BLOCKS', `ifelse(index(confMIN_FREE_BLOCKS, /), -1,,
	`errprint(`*** compound confMIN_FREE_BLOCKS is obsolete.')
	 errprint(`    Use confMAX_MESSAGE_SIZE for the second part of the value.')')')

# clean option definitions below....
define(`_OPTION', `ifdef(`$2', `O $1=$2', `#O $1`'ifelse($3, `',, `=$3')')')dnl

divert(0)dnl

##################
#   local info   #
##################

Cwlocalhost
ifdef(`USE_CW_FILE',
`# file containing names of hosts for which we receive email
Fw`'confCW_FILE',
	`dnl')

# my official domain name
# ... `define' this only if sendmail cannot automatically determine your domain
ifdef(`confDOMAIN_NAME', `Dj`'confDOMAIN_NAME', `#Dj$w.Foo.COM')

ifdef(`_NULL_CLIENT_ONLY_', `divert(-1)')dnl

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
ifdef(`DECNET_RELAY',
`define(`_USE_DECNET_SYNTAX_', 1)dnl
# DECnet relay host
DC`'DECNET_RELAY
CPDECNET

')dnl
ifdef(`FAX_RELAY',
`# FAX relay host
DF`'FAX_RELAY
CPFAX

')dnl
# "Smart" relay host (may be null)
DS`'ifdef(`SMART_HOST', SMART_HOST)

# place to which unknown users should be forwarded
ifdef(`LUSER_RELAY', `', `#')dnl
Kuser user -m -a<>
ifdef(`LUSER_RELAY',
	`DL`'LUSER_RELAY',
	`#DLname_of_luser_relay')

# operators that cannot be in local usernames (i.e., network indicators)
CO @ % ifdef(`_NO_UUCP_', `', `!')

# a class with just dot (for identifying canonical names)
C..

# a class with just a left bracket (for identifying domain literals)
C[[

# Mailer table (overriding domains)
ifdef(`MAILER_TABLE',
	`Kmailertable MAILER_TABLE',
	`#Kmailertable dbm /etc/mailertable')

# Domain table (adding domains)
ifdef(`DOMAIN_TABLE',
	`Kdomaintable DOMAIN_TABLE',
	`#Kdomaintable dbm /etc/domaintable')

# Generics table (mapping outgoing addresses)
ifdef(`GENERICS_TABLE',
	`Kgenerics GENERICS_TABLE',
	`#Kgenerics dbm /etc/genericstable')

# Virtual user table (maps incoming users)
ifdef(`VIRTUSER_TABLE',
	`Kvirtuser VIRTUSER_TABLE',
	`#Kvirtuser dbm /etc/virtusertable')

# who I send unqualified names to (null means deliver locally)
DR`'ifdef(`LOCAL_RELAY', LOCAL_RELAY)

# who gets all local email traffic ($R has precedence for unqualified names)
DH`'ifdef(`MAIL_HUB', MAIL_HUB)

# class L: names that should be delivered locally, even if we have a relay
# class E: names that should be exposed as from this host, even if we masquerade
# class M: domains that should be converted to $M
#CL root
CE root
undivert(5)dnl

# dequoting map
Kdequote dequote

divert(0)dnl	# end of nullclient diversion
# who I masquerade as (null for no masquerading) (see also $=M)
DM`'ifdef(`MASQUERADE_NAME', MASQUERADE_NAME)

# my name for error messages
ifdef(`confMAILER_NAME', `Dn`'confMAILER_NAME', `#DnMAILER-DAEMON')

undivert(6)dnl
include(_CF_DIR_`m4/version.m4')

###############
#   Options   #
###############

# strip message body to 7 bits on input?
_OPTION(SevenBitInput, `confSEVEN_BIT_INPUT')

# 8-bit data handling
_OPTION(EightBitMode, `confEIGHT_BIT_HANDLING', adaptive)

ifdef(`_NULL_CLIENT_ONLY_', `dnl', `
# wait for alias file rebuild (default units: minutes)
_OPTION(AliasWait, `confALIAS_WAIT', 5m)

# location of alias file
_OPTION(AliasFile, `ALIAS_FILE', /etc/aliases)
')
# minimum number of free blocks on filesystem
_OPTION(MinFreeBlocks, `confMIN_FREE_BLOCKS', 100)

# maximum message size
_OPTION(MaxMessageSize, `confMAX_MESSAGE_SIZE', 1000000)

# substitution for space (blank) characters
_OPTION(BlankSub, `confBLANK_SUB', _)

# avoid connecting to "expensive" mailers on initial submission?
_OPTION(HoldExpensive, `confCON_EXPENSIVE')

# checkpoint queue runs after every N successful deliveries
_OPTION(CheckpointInterval, `confCHECKPOINT_INTERVAL', 10)

# default delivery mode
_OPTION(DeliveryMode, `confDELIVERY_MODE', background)

# automatically rebuild the alias database?
_OPTION(AutoRebuildAliases, `confAUTO_REBUILD')

# error message header/file
_OPTION(ErrorHeader, `confERROR_MESSAGE', /etc/sendmail.oE)

# error mode
_OPTION(ErrorMode, `confERROR_MODE', print)

# save Unix-style "From_" lines at top of header?
_OPTION(SaveFromLine, `confSAVE_FROM_LINES')

# temporary file mode
_OPTION(TempFileMode, `confTEMP_FILE_MODE', 0600)

# match recipients against GECOS field?
_OPTION(MatchGECOS, `confMATCH_GECOS')

# maximum hop count
_OPTION(MaxHopCount, `confMAX_HOP', 17)

# location of help file
O HelpFile=ifdef(`HELP_FILE', HELP_FILE, /usr/lib/sendmail.hf)

# ignore dots as terminators in incoming messages?
_OPTION(IgnoreDots, `confIGNORE_DOTS')

# name resolver options
_OPTION(ResolverOptions, `confBIND_OPTS', +AAONLY)

# deliver MIME-encapsulated error messages?
_OPTION(SendMimeErrors, `confMIME_FORMAT_ERRORS')

# Forward file search path
_OPTION(ForwardPath, `confFORWARD_PATH', /var/forward/$u:$z/.forward.$w:$z/.forward)

# open connection cache size
_OPTION(ConnectionCacheSize, `confMCI_CACHE_SIZE', 2)

# open connection cache timeout
_OPTION(ConnectionCacheTimeout, `confMCI_CACHE_TIMEOUT', 5m)

# persistent host status directory
_OPTION(HostStatusDirectory, `confHOST_STATUS_DIRECTORY', .hoststat)

# single thread deliveries (requires HostStatusDirectory)?
_OPTION(SingleThreadDelivery, `confSINGLE_THREAD_DELIVERY')

# use Errors-To: header?
_OPTION(UseErrorsTo, `confUSE_ERRORS_TO')

# log level
_OPTION(LogLevel, `confLOG_LEVEL', 10)

# send to me too, even in an alias expansion?
_OPTION(MeToo, `confME_TOO')

# verify RHS in newaliases?
_OPTION(CheckAliases, `confCHECK_ALIASES')

# default messages to old style headers if no special punctuation?
_OPTION(OldStyleHeaders, `confOLD_STYLE_HEADERS')

# SMTP daemon options
_OPTION(DaemonPortOptions, `confDAEMON_OPTIONS', Port=esmtp)

# privacy flags
_OPTION(PrivacyOptions, `confPRIVACY_FLAGS', authwarnings)

# who (if anyone) should get extra copies of error messages
_OPTION(PostMasterCopy, `confCOPY_ERRORS_TO', Postmaster)

# slope of queue-only function
_OPTION(QueueFactor, `confQUEUE_FACTOR', 600000)

# queue directory
O QueueDirectory=ifdef(`QUEUE_DIR', QUEUE_DIR, /var/spool/mqueue)

# timeouts (many of these)
_OPTION(Timeout.initial, `confTO_INITIAL', 5m)
_OPTION(Timeout.connect, `confTO_CONNECT', 5m)
_OPTION(Timeout.iconnect, `confTO_ICONNECT', 5m)
_OPTION(Timeout.helo, `confTO_HELO', 5m)
_OPTION(Timeout.mail, `confTO_MAIL', 10m)
_OPTION(Timeout.rcpt, `confTO_RCPT', 1h)
_OPTION(Timeout.datainit, `confTO_DATAINIT', 5m)
_OPTION(Timeout.datablock, `confTO_DATABLOCK', 1h)
_OPTION(Timeout.datafinal, `confTO_DATAFINAL', 1h)
_OPTION(Timeout.rset, `confTO_RSET', 5m)
_OPTION(Timeout.quit, `confTO_QUIT', 2m)
_OPTION(Timeout.misc, `confTO_MISC', 2m)
_OPTION(Timeout.command, `confTO_COMMAND', 1h)
_OPTION(Timeout.ident, `confTO_IDENT', 30s)
_OPTION(Timeout.fileopen, `confTO_FILEOPEN', 60s)
_OPTION(Timeout.queuereturn, `confTO_QUEUERETURN', 5d)
_OPTION(Timeout.queuereturn.normal, `confTO_QUEUERETURN_NORMAL', 5d)
_OPTION(Timeout.queuereturn.urgent, `confTO_QUEUERETURN_URGENT', 2d)
_OPTION(Timeout.queuereturn.non-urgent, `confTO_QUEUERETURN_NONURGENT', 7d)
_OPTION(Timeout.queuewarn, `confTO_QUEUEWARN', 4h)
_OPTION(Timeout.queuewarn.normal, `confTO_QUEUEWARN_NORMAL', 4h)
_OPTION(Timeout.queuewarn.urgent, `confTO_QUEUEWARN_URGENT', 1h)
_OPTION(Timeout.queuewarn.non-urgent, `confTO_QUEUEWARN_NONURGENT', 12h)
_OPTION(Timeout.hoststatus, `confTO_HOSTSTATUS', 30m)

# should we not prune routes in route-addr syntax addresses?
_OPTION(DontPruneRoutes, `confDONT_PRUNE_ROUTES')

# queue up everything before forking?
_OPTION(SuperSafe, `confSAFE_QUEUE')

# status file
O StatusFile=ifdef(`STATUS_FILE', `STATUS_FILE', /etc/sendmail.st)

# time zone handling:
#  if undefined, use system default
#  if defined but null, use TZ envariable passed in
#  if defined and non-null, use that info
ifelse(confTIME_ZONE, `USE_SYSTEM', `#O TimeZoneSpec=',
	confTIME_ZONE, `USE_TZ', `O TimeZoneSpec=',
	`O TimeZoneSpec=confTIME_ZONE')

# default UID (can be username or userid:groupid)
_OPTION(DefaultUser, `confDEF_USER_ID', nobody)

# list of locations of user database file (null means no lookup)
_OPTION(UserDatabaseSpec, `confUSERDB_SPEC', /etc/userdb)

# fallback MX host
_OPTION(FallbackMXhost, `confFALLBACK_MX', fall.back.host.net)

# if we are the best MX host for a site, try it directly instead of config err
_OPTION(TryNullMXList, `confTRY_NULL_MX_LIST')

# load average at which we just queue messages
_OPTION(QueueLA, `confQUEUE_LA', 8)

# load average at which we refuse connections
_OPTION(RefuseLA, `confREFUSE_LA', 12)

# maximum number of children we allow at one time
_OPTION(MaxDaemonChildren, `confMAX_DAEMON_CHILDREN', 12)

# maximum number of new connections per second
_OPTION(ConnectionRateThrottle, `confCONNECTION_RATE_THROTTLE', 3)

# work recipient factor
_OPTION(RecipientFactor, `confWORK_RECIPIENT_FACTOR', 30000)

# deliver each queued job in a separate process?
_OPTION(ForkEachJob, `confSEPARATE_PROC')

# work class factor
_OPTION(ClassFactor, `confWORK_CLASS_FACTOR', 1800)

# work time factor
_OPTION(RetryFactor, `confWORK_TIME_FACTOR', 90000)

# shall we sort the queue by hostname first?
_OPTION(QueueSortOrder, `confQUEUE_SORT_ORDER', priority)

# minimum time in queue before retry
_OPTION(MinQueueAge, `confMIN_QUEUE_AGE', 30m)

# default character set
_OPTION(DefaultCharSet, `confDEF_CHAR_SET', iso-8859-1)

# service switch file (ignored on Solaris, Ultrix, OSF/1, others)
_OPTION(ServiceSwitchFile, `confSERVICE_SWITCH_FILE', /etc/service.switch)

# hosts file (normally /etc/hosts)
_OPTION(HostsFile, `confHOSTS_FILE', /etc/hosts)

# dialup line delay on connection failure
_OPTION(DialDelay, `confDIAL_DELAY', 10s)

# action to take if there are no recipients in the message
_OPTION(NoRecipientAction, `confNO_RCPT_ACTION', add-to-undisclosed)

# chrooted environment for writing to files
_OPTION(SafeFileEnvironment, `confSAFE_FILE_ENV', /arch)

# are colons OK in addresses?
_OPTION(ColonOkInAddr, `confCOLON_OK_IN_ADDR')

# how many jobs can you process in the queue?
_OPTION(MaxQueueRunSize, `confMAX_QUEUE_RUN_SIZE', 10000)

# shall I avoid expanding CNAMEs (violates protocols)?
_OPTION(DontExpandCnames, `confDONT_EXPAND_CNAMES')

# SMTP initial login message (old $e macro)
_OPTION(SmtpGreetingMessage, `confSMTP_LOGIN_MSG')

# UNIX initial From header format (old $l macro)
_OPTION(UnixFromLine, `confFROM_LINE')

# delimiter (operator) characters (old $o macro)
_OPTION(OperatorChars, `confOPERATORS')

# shall I avoid calling initgroups(3) because of high NIS costs?
_OPTION(DontInitGroups, `confDONT_INIT_GROUPS')

# are group-writable :include: and .forward files (un)trustworthy?
_OPTION(UnsafeGroupWrites, `confUNSAFE_GROUP_WRITES')

# where do errors that occur when sending errors get sent?
_OPTION(DoubleBounceAddress, `confDOUBLE_BOUNCE_ADDRESS')

# what user id do we assume for the majority of the processing?
_OPTION(RunAsUser, `confRUN_AS_USER', sendmail)

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

# this is equivalent to setting class "t"
ifdef(`_USE_CT_FILE_', `', `#')Ft`'ifdef(`confCT_FILE', confCT_FILE, `/etc/sendmail.ct')
Troot
Tdaemon
ifdef(`_NO_UUCP_', `dnl', `Tuucp')
ifdef(`confTRUSTED_USERS', `T`'confTRUSTED_USERS', `dnl')

#########################
#   Format of headers   #
#########################

ifdef(`confFROM_HEADER',, `define(`confFROM_HEADER', `$?x$x <$g>$|$g$.')')dnl
H?P?Return-Path: <$g>
HReceived: confRECEIVED_HEADER
H?D?Resent-Date: $a
H?D?Date: $a
H?F?Resent-From: confFROM_HEADER
H?F?From: confFROM_HEADER
H?x?Full-Name: $x
# HPosted-Date: $a
# H?l?Received-Date: $b
H?M?Resent-Message-Id: <$t.$i@$j>
H?M?Message-Id: <$t.$i@$j>
ifdef(`_NULL_CLIENT_ONLY_',
	`include(_CF_DIR_`'m4/nullrelay.m4)m4exit',
	`dnl')
#
######################################################################
######################################################################
#####
#####			REWRITING RULES
#####
######################################################################
######################################################################

############################################
###  Ruleset 3 -- Name Canonicalization  ###
############################################
S3

# handle null input (translate to <@> special case)
R$@			$@ <@>

# strip group: syntax (not inside angle brackets!) and trailing semicolon
R$*			$: $1 <@>			mark addresses
R$* < $* > $* <@>	$: $1 < $2 > $3			unmark <addr>
R@ $* <@>		$: @ $1				unmark @host:...
R$* :: $* <@>		$: $1 :: $2			unmark node::addr
R:`include': $* <@>	$: :`include': $1			unmark :`include':...
R$* [ $* : $* ] <@>	$: $1 [ $2 : $3 ]		unmark IPv6 addrs
R$* : $* [ $* ]		$: $1 : $2 [ $3 ] <@>		remark if leading colon
R$* : $* <@>		$: $2				strip colon if marked
R$* <@>			$: $1				unmark
R$* ;			$: $1				strip trailing semi

# null input now results from list:; syntax
R$@			$@ :; <@>

# strip angle brackets -- note RFC733 heuristic to get innermost item
R$*			$: < $1 >			housekeeping <>
R$+ < $* >		   < $2 >			strip excess on left
R< $* > $+		   < $1 >			strip excess on right
R<>			$@ < @ >			MAIL FROM:<> case
R< $+ >			$: $1				remove housekeeping <>

# make sure <@a,@b,@c:user@d> syntax is easy to parse -- undone later
R@ $+ , $+		@ $1 : $2			change all "," to ":"

# localize and dispose of route-based addresses
R@ $+ : $+		$@ $>96 < @$1 > : $2		handle <route-addr>

# find focus for list syntax
R $+ : $* ; @ $+	$@ $>96 $1 : $2 ; < @ $3 >	list syntax
R $+ : $* ;		$@ $1 : $2;			list syntax

# find focus for @ syntax addresses
R$+ @ $+		$: $1 < @ $2 >			focus on domain
R$+ < $+ @ $+ >		$1 $2 < @ $3 >			move gaze right
R$+ < @ $+ >		$@ $>96 $1 < @ $2 >		already canonical

# do some sanity checking
R$* < @ $* : $* > $*	$1 < @ $2 $3 > $4		nix colons in addrs

ifdef(`_NO_UUCP_', `dnl',
`# convert old-style addresses to a domain-based address
R$- ! $+		$@ $>96 $2 < @ $1 .UUCP >	resolve uucp names
R$+ . $- ! $+		$@ $>96 $3 < @ $1 . $2 >		domain uucps
R$+ ! $+		$@ $>96 $2 < @ $1 .UUCP >	uucp subdomains
')
ifdef(`_USE_DECNET_SYNTAX_',
`# convert node::user addresses into a domain-based address
R$- :: $+		$@ $>96 $2 < @ $1 .DECNET >	resolve DECnet names
R$- . $- :: $+		$@ $>96 $3 < @ $1.$2 .DECNET >	numeric DECnet addr
',
	`dnl')
# if we have % signs, take the rightmost one
R$* % $*		$1 @ $2				First make them all @s.
R$* @ $* @ $*		$1 % $2 @ $3			Undo all but the last.
R$* @ $*		$@ $>96 $1 < @ $2 >		Insert < > and finish

# else we must be a local name
R$*			$@ $>96 $1


################################################
###  Ruleset 96 -- bottom half of ruleset 3  ###
################################################

S96

# handle special cases for local names
R$* < @ localhost > $*		$: $1 < @ $j . > $2		no domain at all
R$* < @ localhost . $m > $*	$: $1 < @ $j . > $2		local domain
ifdef(`_NO_UUCP_', `dnl',
`R$* < @ localhost . UUCP > $*	$: $1 < @ $j . > $2		.UUCP domain')
R$* < @ [ $+ ] > $*		$: $1 < @@ [ $2 ] > $3		mark [a.b.c.d]
R$* < @@ $=w > $*		$: $1 < @ $j . > $3		self-literal
R$* < @@ $+ > $*		$@ $1 < @ $2 > $3		canon IP addr

# look up domains in the domain table
ifdef(`DOMAIN_TABLE', `', `#')dnl
R$* < @ $+ > $* 		$: $1 < @ $(domaintable $2 $) > $3

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
R$* < @ $+ . . UUCP . > $*		$@ $1 < @ $2 . > $3')
')
# pass to name server to make hostname canonical
ifdef(`_NO_CANONIFY_', `#')dnl
R$* < @ $* $~P > $*		$: $1 < @ $[ $2 $3 $] > $4

# local host aliases and pseudo-domains are always canonical
R$* < @ $=w > $*		$: $1 < @ $2 . > $3
R$* < @ $j > $*			$: $1 < @ $j . > $2
R$* < @ $* $=M > $*		$: $1 < @ $2 $3 . > $4
R$* < @ $* $=P > $*		$: $1 < @ $2 $3 . > $4
R$* < @ $* . . > $*		$1 < @ $2 . > $3


##################################################
###  Ruleset 4 -- Final Output Post-rewriting  ###
##################################################
S4

R$* <@>			$@				handle <> and list:;

# strip trailing dot off possibly canonical name
R$* < @ $+ . > $*	$1 < @ $2 > $3

# eliminate internal code -- should never get this far!
R$* < @ *LOCAL* > $*	$1 < @ $j > $2

# externalize local domain info
R$* < $+ > $*		$1 $2 $3			defocus
R@ $+ : @ $+ : $+	@ $1 , @ $2 : $3		<route-addr> canonical
R@ $*			$@ @ $1				... and exit

ifdef(`_NO_UUCP_', `dnl',
`# UUCP must always be presented in old form
R$+ @ $- . UUCP		$2!$1				u@h.UUCP => h!u')

ifdef(`_USE_DECNET_SYNTAX_',
`# put DECnet back in :: form
R$+ @ $+ . DECNET	$2 :: $1			u@h.DECNET => h::u',
	`dnl')
# delete duplicate local names
R$+ % $=w @ $=w		$1 @ $j				u%host@host => u@host



##############################################################
###   Ruleset 97 -- recanonicalize and call ruleset zero   ###
###		   (used for recursive calls)		   ###
##############################################################

S`'97
R$*			$: $>3 $1
R$*			$@ $>0 $1


######################################
###   Ruleset 0 -- Parse Address   ###
######################################

S0

R<@>			$#_LOCAL_ $: <@>		special case error msgs
R$* : $* ; <@>		$#error $@ 5.1.3 $: "list:; syntax illegal for recipient addresses"
R<@ $+>			$#error $@ 5.1.1 $: "user address required"
R$*			$: <> $1
R<> $* < @ [ $+ ] > $*	$1 < @ [ $2 ] > $3
R<> $* <$* : $* > $*	$#error $@ 5.1.1 $: "colon illegal in host name part"
R<> $*			$1
R$* < @ . > $*		$#error $@ 5.1.2 $: "invalid host name"

ifdef(`_MAILER_smtp_',
`# handle numeric address spec
R$* < @ [ $+ ] > $*	$: $>98 $1 < @ [ $2 ] > $3	numeric internet spec
R$* < @ [ $+ ] > $*	$#_SMTP_ $@ [$2] $: $1 < @ [$2] > $3	still numeric: send',
	`dnl')

# now delete the local info -- note $=O to find characters that cause forwarding
R$* < @ > $*		$@ $>97 $1		user@ => user
R< @ $=w . > : $*	$@ $>97 $2		@here:... -> ...
R$- < @ $=w . >		$: $(dequote $1 $) < @ $2 . >	dequote "foo"@here
R< @ $+ >		$#error $@ 5.1.1 $: "user address required"
R$* $=O $* < @ $=w . >	$@ $>97 $1 $2 $3		...@here -> ...

# handle local hacks
R$*			$: $>98 $1

# handle virtual users
define(`X', ifdef(`VIRTUSER_TABLE', `', `#'))dnl
X`'R$+ < @ $=w . > 	$: < $(virtuser $1 @ $2 $@ $1 $: @ $) > $1 < @ $2 . >
X`'R< @ > $+ < @ $+ . >	$: < $(virtuser @ $2 $@ $1 $: @ $) > $1 < @ $2 . >
X`'R< @ > $+		$: $1
X`'R< error : $- $+ > $* 	$#error $@ $1 $: $2
X`'R< $+ > $+ < @ $+ >	$: $>97 $1
undefine(`X')dnl

# short circuit local delivery so forwarded email works
ifdef(`_MAILER_usenet_', `', `#')dnl
R$+ . USENET < @ $=w . >	$#usenet $: $1		handle usenet specially
ifdef(`_STICKY_LOCAL_DOMAIN_',
`R$+ < @ $=w . >		$: < $H > $1 < @ $2 . >		first try hub
R< $+ > $+ < $+ >	$>95 < $1 > $2 < $3 >		yep ....
R< > $+ + $* < $+ >	$#_LOCAL_ $: $1 + $2		plussed name?
R< > $+ < $+ >		$#_LOCAL_ $: @ $1			nope, local address',
`R$=L < @ $=w . >	$#_LOCAL_ $: @ $1		special local names
R$+ < @ $=w . >		$#_LOCAL_ $: $1			regular local name')

define(`X', ifdef(`MAILER_TABLE', `', `#'))dnl
# not local -- try mailer table lookup
X`'R$* <@ $+ > $*		$: < $2 > $1 < @ $2 > $3	extract host name
X`'R< $+ . > $*		$: < $1 > $2			strip trailing dot
X`'R< $+ > $*		$: < $(mailertable $1 $) > $2	lookup
X`'R< $~[ : $+ > $* 	$>95 < $1 : $2 > $3		check -- resolved?
X`'R< $+ > $*		$: $>90 <$1> $2			try domain
undefine(`X')dnl
undivert(4)dnl

ifdef(`_NO_UUCP_', `dnl',
`# resolve remotely connected UUCP links (if any)
ifdef(`_CLASS_V_',
`R$* < @ $=V . UUCP . > $*		$: $>95 < $V > $1 <@$2.UUCP.> $3',
	`dnl')
ifdef(`_CLASS_W_',
`R$* < @ $=W . UUCP . > $*		$: $>95 < $W > $1 <@$2.UUCP.> $3',
	`dnl')
ifdef(`_CLASS_X_',
`R$* < @ $=X . UUCP . > $*		$: $>95 < $X > $1 <@$2.UUCP.> $3',
	`dnl')')

# resolve fake top level domains by forwarding to other hosts
ifdef(`BITNET_RELAY',
`R$*<@$+.BITNET.>$*	$: $>95 < $B > $1 <@$2.BITNET.> $3	user@host.BITNET',
	`dnl')
ifdef(`DECNET_RELAY',
`R$*<@$+.DECNET.>$*	$: $>95 < $C > $1 <@$2.DECNET.> $3	user@host.DECNET',
	`dnl')
ifdef(`_MAILER_pop_',
`R$+ < @ POP. >		$#pop $: $1			user@POP',
	`dnl')
ifdef(`_MAILER_fax_',
`R$+ < @ $+ .FAX. >	$#fax $@ $2 $: $1		user@host.FAX',
`ifdef(`FAX_RELAY',
`R$*<@$+.FAX.>$*		$: $>95 < $F > $1 <@$2.FAX.> $3	user@host.FAX',
	`dnl')')

ifdef(`UUCP_RELAY',
`# forward non-local UUCP traffic to our UUCP relay
R$*<@$*.UUCP.>$*		$: $>95 < $Y > $1 <@$2.UUCP.> $3	uucp mail',
`ifdef(`_MAILER_uucp_',
`# forward other UUCP traffic straight to UUCP
R$* < @ $+ .UUCP. > $*		$#_UUCP_ $@ $2 $: $1 < @ $2 .UUCP. > $3	user@host.UUCP',
	`dnl')')
ifdef(`_MAILER_usenet_', `
# addresses sent to net.group.USENET will get forwarded to a newsgroup
R$+ . USENET		$#usenet $: $1',
	`dnl')

ifdef(`_LOCAL_RULES_',
`# figure out what should stay in our local mail system
undivert(1)', `dnl')

# pass names that still have a host to a smarthost (if defined)
R$* < @ $* > $*		$: $>95 < $S > $1 < @ $2 > $3	glue on smarthost name

# deal with other remote names
ifdef(`_MAILER_smtp_',
`R$* < @$* > $*		$#_SMTP_ $@ $2 $: $1 < @ $2 > $3		user@host.domain',
`R$* < @$* > $*		$#error $@ 5.1.2 $: Unrecognized host name $2')

# if this is quoted, strip the quotes and try again
R$+			$: $(dequote $1 $)		strip quotes
R$+ $=O $+		$@ $>97 $1 $2 $3			try again

# handle locally delivered names
R$=L			$#_LOCAL_ $: @ $1			special local names
R$+			$#_LOCAL_ $: $1			regular local names

###########################################################################
###   Ruleset 5 -- special rewriting after aliases have been expanded   ###
###########################################################################

S5

# deal with plussed users so aliases work nicely
R$+ + *			$#_LOCAL_ $@ $&h $: $1
R$+ + $*		$#_LOCAL_ $@ $2 $: $1 + *

# prepend an empty "forward host" on the front
R$+			$: <> $1

define(`X', ifdef(`LUSER_RELAY', `', `#'))dnl
# send unrecognized local users to a relay host
X`'R< > $+ + $*		$: < $L . > $( user $1 $) + $2
X`'R< > $+ 		$: < $L . > $( user $1 $)	look up user
X`'R< $* > $+ <> $*	$: < > $2 $3			found; strip $L
X`'R< $* . > $+		$: < $1 > $2			strip extra dot
undefine(`X')dnl

# handle plussed local names
R< > $+ + $*		$#_LOCAL_ $@ $2 $: $1

# see if we have a relay or a hub
R< > $+			$: < $H > $1			try hub
R< > $+			$: < $R > $1			try relay
R< > $+			$@ $1				nope, give up
R< $- : $+ > $+		$: $>95 < $1 : $2 > $3 < @ $2 >
R< $+ > $+		$@ $>95 < $1 > $2 < @ $1 >

###################################################################
###  Ruleset 90 -- try domain part of mailertable entry 	###
###################################################################

define(`X', ifdef(`MAILER_TABLE', `', `#'))dnl
S90
X`'R$* <$- . $+ > $*	$: $1$2 < $(mailertable .$3 $@ $1$2 $@ $2 $) > $4
X`'R$* <$~[ : $+ > $*		$>95 < $2 : $3 > $4	check -- resolved?
X`'R$* < . $+ > $* 		$@ $>90 $1 . <$2> $3	no -- strip & try again
X`'R$* < $* > $*		$: < $(mailertable . $@ $1$2 $) > $3	try "."
X`'R< $~[ : $+ > $*		$>95 < $1 : $2 > $3	"." found?
X`'R< $* > $*			$@ $2			no mailertable match
undefine(`X')dnl

###################################################################
###  Ruleset 95 -- canonify mailer:[user@]host syntax to triple	###
###################################################################

S95
R< > $*				$@ $1			strip off null relay
R< error : $- $+ > $*		$#error $@ $1 $: $2	special case errors
R< local : > $* < @ $* >	$#local $@ $1@$2 $: $1	no host: use old user
R< local : $+ > $* <@ $* . > $*	$#local $@ $2@$3 $: $1	special case local
R< $- : $+ @ $+ > $*<$*>$*	$# $1 $@ $3 $: $2<@$3>	use literal user
R< $- : $+ > $*			$# $1 $@ $2 $: $3	try qualified mailer
R< $=w > $*			$@ $2			delete local host
R< $+ > $*			$#_RELAY_ $@ $1 $: $2	use unqualified mailer

###################################################################
###  Ruleset 93 -- convert header names to masqueraded form	###
###################################################################

S93

# handle generics database
define(`X', ifdef(`GENERICS_TABLE', `', `#'))dnl
X`'R$+ < @ $=G . > 	$: < $1@$2 > $1 < @ $2 . > @	mark
X`'R$+ < @ *LOCAL* >	$: < $1@$j > $1 < @ *LOCAL* > @	mark
X`'R< $+ > $+ < $* > @	$: < $(generics $1 $: $) > $2 < $3 >
X`'R< > $+ < @ $+ > 	$: < $(generics $1 $: $) > $1 < @ $2 >
X`'R< $* @ $* > $* < $* >	$@ $>3 $1 @ $2			found qualified
X`'R< $+ > $* < $* >	$: $>3 $1 @ *LOCAL*		found unqualified
X`'R< > $*			$: $1				not found
undefine(`X')dnl

# special case the users that should be exposed
R$=E < @ *LOCAL* >	$@ $1 < @ $j . >		leave exposed
ifdef(`_MASQUERADE_ENTIRE_DOMAIN_',
`R$=E < @ $* $=M . >	$@ $1 < @ $2 $3 . >',
`R$=E < @ $=M . >	$@ $1 < @ $2 . >')
ifdef(`_LIMITED_MASQUERADE_', `#')dnl
R$=E < @ $=w . >	$@ $1 < @ $2 . >

# handle domain-specific masquerading
ifdef(`_MASQUERADE_ENTIRE_DOMAIN_',
`R$* < @ $* $=M . > $*	$: $1 < @ $2 $3 . @ $M > $4	convert masqueraded doms',
`R$* < @ $=M . > $*	$: $1 < @ $2 . @ $M > $3	convert masqueraded doms')
ifdef(`_LIMITED_MASQUERADE_', `#')dnl
R$* < @ $=w . > $*	$: $1 < @ $2 . @ $M > $3
R$* < @ *LOCAL* > $*	$: $1 < @ $j . @ $M > $2
R$* < @ $+ @ > $*	$: $1 < @ $2 > $3		$M is null
R$* < @ $+ @ $+ > $*	$: $1 < @ $3 . > $4		$M is not null

###################################################################
###  Ruleset 94 -- convert envelope names to masqueraded form	###
###################################################################

S94
ifdef(`_MASQUERADE_ENVELOPE_', `', `#')dnl
R$+			$@ $>93 $1
ifdef(`_MASQUERADE_ENVELOPE_', `#', `')dnl
R$* < @ *LOCAL* > $*	$: $1 < @ $j . > $2

###################################################################
###  Ruleset 98 -- local part of ruleset zero (can be null)	###
###################################################################

S98
undivert(3)dnl
undivert(9)dnl
#
######################################################################
######################################################################
#####
`#####			MAILER DEFINITIONS'
#####
######################################################################
######################################################################
undivert(7)dnl
