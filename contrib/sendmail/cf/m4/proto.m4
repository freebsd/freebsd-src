divert(-1)
#
# Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983, 1995 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
divert(0)

VERSIONID(`$Id: proto.m4,v 8.446.2.5.2.29 2000/09/15 04:45:14 gshapiro Exp $')

MAILER(local)dnl

# level CF_LEVEL config file format
V`'CF_LEVEL/ifdef(`VENDOR_NAME', `VENDOR_NAME', `Berkeley')
divert(-1)

# do some sanity checking
ifdef(`__OSTYPE__',,
	`errprint(`*** ERROR: No system type defined (use OSTYPE macro)
')')

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
`errprint(`*** confDEF_GROUP_ID is obsolete.
    Use confDEF_USER_ID with a colon in the value instead.
')')
ifdef(`confREAD_TIMEOUT',
`errprint(`*** confREAD_TIMEOUT is obsolete.
    Use individual confTO_<timeout> parameters instead.
')')
ifdef(`confMESSAGE_TIMEOUT',
	`define(`_ARG_', index(confMESSAGE_TIMEOUT, /))
	 ifelse(_ARG_, -1,
		`define(`confTO_QUEUERETURN', confMESSAGE_TIMEOUT)',
		`define(`confTO_QUEUERETURN',
			substr(confMESSAGE_TIMEOUT, 0, _ARG_))
		 define(`confTO_QUEUEWARN',
			substr(confMESSAGE_TIMEOUT, eval(_ARG_+1)))')')
ifdef(`confMIN_FREE_BLOCKS', `ifelse(index(confMIN_FREE_BLOCKS, /), -1,,
`errprint(`*** compound confMIN_FREE_BLOCKS is obsolete.
    Use confMAX_MESSAGE_SIZE for the second part of the value.
')')')


# Sanity check on ldap_routing feature
# If the user doesn't specify a new map, they better have given as a
# default LDAP specification which has the LDAP base (and most likely the host)
ifdef(`confLDAP_DEFAULT_SPEC',, `ifdef(`_LDAP_ROUTING_WARN_', `errprint(`
WARNING: Using default FEATURE(ldap_routing) map definition(s)
without setting confLDAP_DEFAULT_SPEC option.
')')')dnl

# clean option definitions below....
define(`_OPTION', `ifdef(`$2', `O $1`'ifelse(defn(`$2'), `',, `=$2')', `#O $1`'ifelse(`$3', `',,`=$3')')')dnl

dnl required to "rename" the check_* rulesets...
define(`_U_',ifdef(`_DELAY_CHECKS_',`',`_'))
dnl default relaying denied message
ifdef(`confRELAY_MSG', `', `define(`confRELAY_MSG', `"550 Relaying denied"')')
divert(0)dnl

# override file safeties - setting this option compromises system security,
# addressing the actual file configuration problem is preferred
# need to set this before any file actions are encountered in the cf file
_OPTION(DontBlameSendmail, `confDONT_BLAME_SENDMAIL', `safe')

# default LDAP map specification
# need to set this now before any LDAP maps are defined
_OPTION(LDAPDefaultSpec, `confLDAP_DEFAULT_SPEC', `-h localhost')

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

ifdef(`LUSER_RELAY', `dnl
# place to which unknown users should be forwarded
Kuser user -m -a<>
DL`'LUSER_RELAY',
`dnl')

# operators that cannot be in local usernames (i.e., network indicators)
CO @ % ifdef(`_NO_UUCP_', `', `!')

# a class with just dot (for identifying canonical names)
C..

# a class with just a left bracket (for identifying domain literals)
C[[

ifdef(`_ACCESS_TABLE_', `dnl
# access_db acceptance class
C{Accept}OK RELAY
ifdef(`_DELAY_CHECKS_',`dnl
ifdef(`_BLACKLIST_RCPT_',`dnl
# possible access_db RHS for spam friends/haters
C{SpamTag}SPAMFRIEND SPAMHATER')')',
`dnl')

ifdef(`_ACCEPT_UNRESOLVABLE_DOMAINS_',`dnl',`dnl
# Resolve map (to check if a host exists in check_mail)
Kresolve host -a<OK> -T<TEMP>')

ifdef(`_FFR_5_', `# macro storage map
Kmacro macro')

ifdef(`confCR_FILE', `dnl
# Hosts for which relaying is permitted ($=R)
FR`'confCR_FILE',
`dnl')

define(`TLS_SRV_TAG', `TLS_Srv')dnl
define(`TLS_CLT_TAG', `TLS_Clt')dnl
define(`TLS_TRY_TAG', `Try_TLS')dnl
define(`TLS_OFF_TAG', `Offer_TLS')dnl
dnl this may be useful in other contexts too
ifdef(`_ARITH_MAP_', `', `# arithmetic map
define(`_ARITH_MAP_', `1')dnl
Karith arith')
ifdef(`_ACCESS_TABLE_', `dnl
# possible values for tls_connect in access map
C{tls}VERIFY ENCR', `dnl')
ifdef(`_CERT_REGEX_ISSUER_', `dnl
# extract relevant part from cert issuer
KCERTIssuer regex _CERT_REGEX_ISSUER_', `dnl')
ifdef(`_CERT_REGEX_SUBJECT_', `dnl
# extract relevant part from cert subject
KCERTSubject regex _CERT_REGEX_SUBJECT_', `dnl')

# who I send unqualified names to (null means deliver locally)
DR`'ifdef(`LOCAL_RELAY', LOCAL_RELAY)

# who gets all local email traffic ($R has precedence for unqualified names)
DH`'ifdef(`MAIL_HUB', MAIL_HUB)

# dequoting map
Kdequote dequote

divert(0)dnl	# end of nullclient diversion
# class E: names that should be exposed as from this host, even if we masquerade
# class L: names that should be delivered locally, even if we have a relay
# class M: domains that should be converted to $M
# class N: domains that should not be converted to $M
#CL root
undivert(5)dnl
ifdef(`_VIRTHOSTS_', `CR$={VirtHost}', `dnl')

# who I masquerade as (null for no masquerading) (see also $=M)
DM`'ifdef(`MASQUERADE_NAME', MASQUERADE_NAME)

# my name for error messages
ifdef(`confMAILER_NAME', `Dn`'confMAILER_NAME', `#DnMAILER-DAEMON')

undivert(6)dnl LOCAL_CONFIG
include(_CF_DIR_`m4/version.m4')

###############
#   Options   #
###############

# strip message body to 7 bits on input?
_OPTION(SevenBitInput, `confSEVEN_BIT_INPUT', `False')

# 8-bit data handling
_OPTION(EightBitMode, `confEIGHT_BIT_HANDLING', `adaptive')

# wait for alias file rebuild (default units: minutes)
_OPTION(AliasWait, `confALIAS_WAIT', `5m')

# location of alias file
_OPTION(AliasFile, `ALIAS_FILE', `MAIL_SETTINGS_DIR`'aliases')

# minimum number of free blocks on filesystem
_OPTION(MinFreeBlocks, `confMIN_FREE_BLOCKS', `100')

# maximum message size
_OPTION(MaxMessageSize, `confMAX_MESSAGE_SIZE', `1000000')

# substitution for space (blank) characters
_OPTION(BlankSub, `confBLANK_SUB', `_')

# avoid connecting to "expensive" mailers on initial submission?
_OPTION(HoldExpensive, `confCON_EXPENSIVE', `False')

# checkpoint queue runs after every N successful deliveries
_OPTION(CheckpointInterval, `confCHECKPOINT_INTERVAL', `10')

# default delivery mode
_OPTION(DeliveryMode, `confDELIVERY_MODE', `background')

# automatically rebuild the alias database?
# NOTE: There is a potential for a denial of service attack if this is set.
#       This option is deprecated and will be removed from a future version.
_OPTION(AutoRebuildAliases, `confAUTO_REBUILD', `False')

# error message header/file
_OPTION(ErrorHeader, `confERROR_MESSAGE', `MAIL_SETTINGS_DIR`'error-header')

# error mode
_OPTION(ErrorMode, `confERROR_MODE', `print')

# save Unix-style "From_" lines at top of header?
_OPTION(SaveFromLine, `confSAVE_FROM_LINES', `False')

# temporary file mode
_OPTION(TempFileMode, `confTEMP_FILE_MODE', `0600')

# match recipients against GECOS field?
_OPTION(MatchGECOS, `confMATCH_GECOS', `False')

# maximum hop count
_OPTION(MaxHopCount, `confMAX_HOP', `17')

# location of help file
O HelpFile=ifdef(`HELP_FILE', HELP_FILE, `MAIL_SETTINGS_DIR`'helpfile')

# ignore dots as terminators in incoming messages?
_OPTION(IgnoreDots, `confIGNORE_DOTS', `False')

# name resolver options
_OPTION(ResolverOptions, `confBIND_OPTS', `+AAONLY')

# deliver MIME-encapsulated error messages?
_OPTION(SendMimeErrors, `confMIME_FORMAT_ERRORS', `True')

# Forward file search path
_OPTION(ForwardPath, `confFORWARD_PATH', `/var/forward/$u:$z/.forward.$w:$z/.forward')

# open connection cache size
_OPTION(ConnectionCacheSize, `confMCI_CACHE_SIZE', `2')

# open connection cache timeout
_OPTION(ConnectionCacheTimeout, `confMCI_CACHE_TIMEOUT', `5m')

# persistent host status directory
_OPTION(HostStatusDirectory, `confHOST_STATUS_DIRECTORY', `.hoststat')

# single thread deliveries (requires HostStatusDirectory)?
_OPTION(SingleThreadDelivery, `confSINGLE_THREAD_DELIVERY', `False')

# use Errors-To: header?
_OPTION(UseErrorsTo, `confUSE_ERRORS_TO', `False')

# log level
_OPTION(LogLevel, `confLOG_LEVEL', `10')

# send to me too, even in an alias expansion?
_OPTION(MeToo, `confME_TOO', `True')

# verify RHS in newaliases?
_OPTION(CheckAliases, `confCHECK_ALIASES', `False')

# default messages to old style headers if no special punctuation?
_OPTION(OldStyleHeaders, `confOLD_STYLE_HEADERS', `False')

# SMTP daemon options
ifelse(defn(`confDAEMON_OPTIONS'), `', `dnl',
`errprint(WARNING: `confDAEMON_OPTIONS' is no longer valid.  See cf/README for more information.
)'dnl
`DAEMON_OPTIONS(`confDAEMON_OPTIONS')')
ifelse(defn(`_DPO_'), `',
`ifdef(`_NETINET6_', `O DaemonPortOptions=Name=MTA-IPv4, Family=inet
O DaemonPortOptions=Name=MTA-IPv6, Family=inet6',`O DaemonPortOptions=Name=MTA')', `_DPO_')
ifdef(`_NO_MSA_', `dnl', `O DaemonPortOptions=Port=587, Name=MSA, M=E')

# SMTP client options
_OPTION(ClientPortOptions, `confCLIENT_OPTIONS', `Address=0.0.0.0')

# privacy flags
_OPTION(PrivacyOptions, `confPRIVACY_FLAGS', `authwarnings')

# who (if anyone) should get extra copies of error messages
_OPTION(PostmasterCopy, `confCOPY_ERRORS_TO', `Postmaster')

# slope of queue-only function
_OPTION(QueueFactor, `confQUEUE_FACTOR', `600000')

# queue directory
O QueueDirectory=ifdef(`QUEUE_DIR', QUEUE_DIR, `/var/spool/mqueue')

# timeouts (many of these)
_OPTION(Timeout.initial, `confTO_INITIAL', `5m')
_OPTION(Timeout.connect, `confTO_CONNECT', `5m')
_OPTION(Timeout.iconnect, `confTO_ICONNECT', `5m')
_OPTION(Timeout.helo, `confTO_HELO', `5m')
_OPTION(Timeout.mail, `confTO_MAIL', `10m')
_OPTION(Timeout.rcpt, `confTO_RCPT', `1h')
_OPTION(Timeout.datainit, `confTO_DATAINIT', `5m')
_OPTION(Timeout.datablock, `confTO_DATABLOCK', `1h')
_OPTION(Timeout.datafinal, `confTO_DATAFINAL', `1h')
_OPTION(Timeout.rset, `confTO_RSET', `5m')
_OPTION(Timeout.quit, `confTO_QUIT', `2m')
_OPTION(Timeout.misc, `confTO_MISC', `2m')
_OPTION(Timeout.command, `confTO_COMMAND', `1h')
_OPTION(Timeout.ident, `confTO_IDENT', `5s')
_OPTION(Timeout.fileopen, `confTO_FILEOPEN', `60s')
_OPTION(Timeout.control, `confTO_CONTROL', `2m')
_OPTION(Timeout.queuereturn, `confTO_QUEUERETURN', `5d')
_OPTION(Timeout.queuereturn.normal, `confTO_QUEUERETURN_NORMAL', `5d')
_OPTION(Timeout.queuereturn.urgent, `confTO_QUEUERETURN_URGENT', `2d')
_OPTION(Timeout.queuereturn.non-urgent, `confTO_QUEUERETURN_NONURGENT', `7d')
_OPTION(Timeout.queuewarn, `confTO_QUEUEWARN', `4h')
_OPTION(Timeout.queuewarn.normal, `confTO_QUEUEWARN_NORMAL', `4h')
_OPTION(Timeout.queuewarn.urgent, `confTO_QUEUEWARN_URGENT', `1h')
_OPTION(Timeout.queuewarn.non-urgent, `confTO_QUEUEWARN_NONURGENT', `12h')
_OPTION(Timeout.hoststatus, `confTO_HOSTSTATUS', `30m')
_OPTION(Timeout.resolver.retrans, `confTO_RESOLVER_RETRANS', `5s')
_OPTION(Timeout.resolver.retrans.first, `confTO_RESOLVER_RETRANS_FIRST', `5s')
_OPTION(Timeout.resolver.retrans.normal, `confTO_RESOLVER_RETRANS_NORMAL', `5s')
_OPTION(Timeout.resolver.retry, `confTO_RESOLVER_RETRY', `4')
_OPTION(Timeout.resolver.retry.first, `confTO_RESOLVER_RETRY_FIRST', `4')
_OPTION(Timeout.resolver.retry.normal, `confTO_RESOLVER_RETRY_NORMAL', `4')

# should we not prune routes in route-addr syntax addresses?
_OPTION(DontPruneRoutes, `confDONT_PRUNE_ROUTES', `False')

# queue up everything before forking?
_OPTION(SuperSafe, `confSAFE_QUEUE', `True')

# status file
O StatusFile=ifdef(`STATUS_FILE', `STATUS_FILE', `MAIL_SETTINGS_DIR`'statistics')

# time zone handling:
#  if undefined, use system default
#  if defined but null, use TZ envariable passed in
#  if defined and non-null, use that info
ifelse(confTIME_ZONE, `USE_SYSTEM', `#O TimeZoneSpec=',
	confTIME_ZONE, `USE_TZ', `O TimeZoneSpec=',
	`O TimeZoneSpec=confTIME_ZONE')

# default UID (can be username or userid:groupid)
_OPTION(DefaultUser, `confDEF_USER_ID', `mailnull')

# list of locations of user database file (null means no lookup)
_OPTION(UserDatabaseSpec, `confUSERDB_SPEC', `MAIL_SETTINGS_DIR`'userdb')

# fallback MX host
_OPTION(FallbackMXhost, `confFALLBACK_MX', `fall.back.host.net')

# if we are the best MX host for a site, try it directly instead of config err
_OPTION(TryNullMXList, `confTRY_NULL_MX_LIST', `False')

# load average at which we just queue messages
_OPTION(QueueLA, `confQUEUE_LA', `8')

# load average at which we refuse connections
_OPTION(RefuseLA, `confREFUSE_LA', `12')

# maximum number of children we allow at one time
_OPTION(MaxDaemonChildren, `confMAX_DAEMON_CHILDREN', `12')

# maximum number of new connections per second
_OPTION(ConnectionRateThrottle, `confCONNECTION_RATE_THROTTLE', `3')

# work recipient factor
_OPTION(RecipientFactor, `confWORK_RECIPIENT_FACTOR', `30000')

# deliver each queued job in a separate process?
_OPTION(ForkEachJob, `confSEPARATE_PROC', `False')

# work class factor
_OPTION(ClassFactor, `confWORK_CLASS_FACTOR', `1800')

# work time factor
_OPTION(RetryFactor, `confWORK_TIME_FACTOR', `90000')

# shall we sort the queue by hostname first?
_OPTION(QueueSortOrder, `confQUEUE_SORT_ORDER', `priority')

# minimum time in queue before retry
_OPTION(MinQueueAge, `confMIN_QUEUE_AGE', `30m')

# default character set
_OPTION(DefaultCharSet, `confDEF_CHAR_SET', `iso-8859-1')

# service switch file (ignored on Solaris, Ultrix, OSF/1, others)
_OPTION(ServiceSwitchFile, `confSERVICE_SWITCH_FILE', `MAIL_SETTINGS_DIR`'service.switch')

# hosts file (normally /etc/hosts)
_OPTION(HostsFile, `confHOSTS_FILE', `/etc/hosts')

# dialup line delay on connection failure
_OPTION(DialDelay, `confDIAL_DELAY', `10s')

# action to take if there are no recipients in the message
_OPTION(NoRecipientAction, `confNO_RCPT_ACTION', `add-to-undisclosed')

# chrooted environment for writing to files
_OPTION(SafeFileEnvironment, `confSAFE_FILE_ENV', `/arch')

# are colons OK in addresses?
_OPTION(ColonOkInAddr, `confCOLON_OK_IN_ADDR', `True')

# how many jobs can you process in the queue?
_OPTION(MaxQueueRunSize, `confMAX_QUEUE_RUN_SIZE', `10000')

# shall I avoid expanding CNAMEs (violates protocols)?
_OPTION(DontExpandCnames, `confDONT_EXPAND_CNAMES', `False')

# SMTP initial login message (old $e macro)
_OPTION(SmtpGreetingMessage, `confSMTP_LOGIN_MSG', `$j Sendmail $v ready at $b')

# UNIX initial From header format (old $l macro)
_OPTION(UnixFromLine, `confFROM_LINE', `From $g $d')

# From: lines that have embedded newlines are unwrapped onto one line
_OPTION(SingleLineFromHeader, `confSINGLE_LINE_FROM_HEADER', `False')

# Allow HELO SMTP command that does not `include' a host name
_OPTION(AllowBogusHELO, `confALLOW_BOGUS_HELO', `False')

# Characters to be quoted in a full name phrase (@,;:\()[] are automatic)
_OPTION(MustQuoteChars, `confMUST_QUOTE_CHARS', `.')

# delimiter (operator) characters (old $o macro)
_OPTION(OperatorChars, `confOPERATORS', `.:@[]')

# shall I avoid calling initgroups(3) because of high NIS costs?
_OPTION(DontInitGroups, `confDONT_INIT_GROUPS', `False')

# are group-writable `:include:' and .forward files (un)trustworthy?
_OPTION(UnsafeGroupWrites, `confUNSAFE_GROUP_WRITES', `True')

# where do errors that occur when sending errors get sent?
_OPTION(DoubleBounceAddress, `confDOUBLE_BOUNCE_ADDRESS', `postmaster')

# where to save bounces if all else fails
_OPTION(DeadLetterDrop, `confDEAD_LETTER_DROP', `/var/tmp/dead.letter')

# what user id do we assume for the majority of the processing?
_OPTION(RunAsUser, `confRUN_AS_USER', `sendmail')

# maximum number of recipients per SMTP envelope
_OPTION(MaxRecipientsPerMessage, `confMAX_RCPTS_PER_MESSAGE', `100')

# shall we get local names from our installed interfaces?
_OPTION(DontProbeInterfaces, `confDONT_PROBE_INTERFACES', `False')

# Return-Receipt-To: header implies DSN request
_OPTION(RrtImpliesDsn, `confRRT_IMPLIES_DSN', `False')

# override connection address (for testing)
_OPTION(ConnectOnlyTo, `confCONNECT_ONLY_TO', `0.0.0.0')

# Trusted user for file ownership and starting the daemon
_OPTION(TrustedUser, `confTRUSTED_USER', `root')

# Control socket for daemon management
_OPTION(ControlSocketName, `confCONTROL_SOCKET_NAME', `/var/spool/mqueue/.control')

# Maximum MIME header length to protect MUAs
_OPTION(MaxMimeHeaderLength, `confMAX_MIME_HEADER_LENGTH', `0/0')

# Maximum length of the sum of all headers
_OPTION(MaxHeadersLength, `confMAX_HEADERS_LENGTH', `32768')

# Maximum depth of alias recursion
_OPTION(MaxAliasRecursion, `confMAX_ALIAS_RECURSION', `10')

# location of pid file
_OPTION(PidFile, `confPID_FILE', `/var/run/sendmail.pid')

# Prefix string for the process title shown on 'ps' listings
_OPTION(ProcessTitlePrefix, `confPROCESS_TITLE_PREFIX', `prefix')

# Data file (df) memory-buffer file maximum size
_OPTION(DataFileBufferSize, `confDF_BUFFER_SIZE', `4096')

# Transcript file (xf) memory-buffer file maximum size
_OPTION(XscriptFileBufferSize, `confXF_BUFFER_SIZE', `4096')

# list of authentication mechanisms
_OPTION(AuthMechanisms, `confAUTH_MECHANISMS', `GSSAPI KERBEROS_V4 DIGEST-MD5 CRAM-MD5')

# default authentication information for outgoing connections
_OPTION(DefaultAuthInfo, `confDEF_AUTH_INFO', `MAIL_SETTINGS_DIR`'default-auth-info')

# SMTP AUTH flags
_OPTION(AuthOptions, `confAUTH_OPTIONS', `')

ifdef(`_FFR_MILTER', `
# Input mail filters
_OPTION(InputMailFilters, `confINPUT_MAIL_FILTERS', `')

# Milter options
_OPTION(Milter.macros.connect, `confMILTER_MACROS_CONNECT', `')
_OPTION(Milter.macros.helo, `confMILTER_MACROS_HELO', `')
_OPTION(Milter.macros.envfrom, `confMILTER_MACROS_ENVFROM', `')
_OPTION(Milter.macros.envrcpt, `confMILTER_MACROS_ENVRCPT', `')')

# CA directory
_OPTION(CACERTPath, `confCACERT_PATH', `')
# CA file
_OPTION(CACERTFile, `confCACERT', `')
# Server Cert
_OPTION(ServerCertFile, `confSERVER_CERT', `')
# Server private key
_OPTION(ServerKeyFile, `confSERVER_KEY', `')
# Client Cert
_OPTION(ClientCertFile, `confCLIENT_CERT', `')
# Client private key
_OPTION(ClientKeyFile, `confCLIENT_KEY', `')
# DHParameters (only required if DSA/DH is used)
_OPTION(DHParameters, `confDH_PARAMETERS', `')
# Random data source (required for systems without /dev/urandom under OpenSSL)
_OPTION(RandFile, `confRAND_FILE', `')

ifdef(`confQUEUE_FILE_MODE',
`# queue file mode (qf files)
O QueueFileMode=confQUEUE_FILE_MODE
')

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
ifdef(`_USE_CT_FILE_', `', `#')Ft`'ifdef(`confCT_FILE', confCT_FILE, `MAIL_SETTINGS_DIR`'trusted-users')
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
Scanonify=3

# handle null input (translate to <@> special case)
R$@			$@ <@>

# strip group: syntax (not inside angle brackets!) and trailing semicolon
R$*			$: $1 <@>			mark addresses
R$* < $* > $* <@>	$: $1 < $2 > $3			unmark <addr>
R@ $* <@>		$: @ $1				unmark @host:...
R$* :: $* <@>		$: $1 :: $2			unmark node::addr
R:`include': $* <@>	$: :`include': $1			unmark :`include':...
R$* [ IPv6 $- ] <@>	$: $1 [ IPv6 $2 ]		unmark IPv6 addr
R$* : $* [ $* ]		$: $1 : $2 [ $3 ] <@>		remark if leading colon
R$* : $* <@>		$: $2				strip colon if marked
R$* <@>			$: $1				unmark
R$* ;			   $1				strip trailing semi
R$* < $* ; >		   $1 < $2 >			bogus bracketed semi

# null input now results from list:; syntax
R$@			$@ :; <@>

# strip angle brackets -- note RFC733 heuristic to get innermost item
R$*			$: < $1 >			housekeeping <>
R$+ < $* >		   < $2 >			strip excess on left
R< $* > $+		   < $1 >			strip excess on right
R<>			$@ < @ >			MAIL FROM:<> case
R< $+ >			$: $1				remove housekeeping <>

ifdef(`_USE_DEPRECATED_ROUTE_ADDR_',`dnl
# make sure <@a,@b,@c:user@d> syntax is easy to parse -- undone later
R@ $+ , $+		@ $1 : $2			change all "," to ":"

# localize and dispose of route-based addresses
R@ $+ : $+		$@ $>Canonify2 < @$1 > : $2	handle <route-addr>
dnl',`dnl
# strip route address <@a,@b,@c:user@d> -> <user@d>
R@ $+ , $+		$2
R@ $+ : $+		$2
dnl')

# find focus for list syntax
R $+ : $* ; @ $+	$@ $>Canonify2 $1 : $2 ; < @ $3 >	list syntax
R $+ : $* ;		$@ $1 : $2;			list syntax

# find focus for @ syntax addresses
R$+ @ $+		$: $1 < @ $2 >			focus on domain
R$+ < $+ @ $+ >		$1 $2 < @ $3 >			move gaze right
R$+ < @ $+ >		$@ $>Canonify2 $1 < @ $2 >	already canonical

# do some sanity checking
R$* < @ $* : $* > $*	$1 < @ $2 $3 > $4		nix colons in addrs

ifdef(`_NO_UUCP_', `dnl',
`# convert old-style addresses to a domain-based address
R$- ! $+		$@ $>Canonify2 $2 < @ $1 .UUCP >	resolve uucp names
R$+ . $- ! $+		$@ $>Canonify2 $3 < @ $1 . $2 >		domain uucps
R$+ ! $+		$@ $>Canonify2 $2 < @ $1 .UUCP >	uucp subdomains
')
ifdef(`_USE_DECNET_SYNTAX_',
`# convert node::user addresses into a domain-based address
R$- :: $+		$@ $>Canonify2 $2 < @ $1 .DECNET >	resolve DECnet names
R$- . $- :: $+		$@ $>Canonify2 $3 < @ $1.$2 .DECNET >	numeric DECnet addr
',
	`dnl')
# if we have % signs, take the rightmost one
R$* % $*		$1 @ $2				First make them all @s.
R$* @ $* @ $*		$1 % $2 @ $3			Undo all but the last.
R$* @ $*		$@ $>Canonify2 $1 < @ $2 >	Insert < > and finish

# else we must be a local name
R$*			$@ $>Canonify2 $1


################################################
###  Ruleset 96 -- bottom half of ruleset 3  ###
################################################

SCanonify2=96

# handle special cases for local names
R$* < @ localhost > $*		$: $1 < @ $j . > $2		no domain at all
R$* < @ localhost . $m > $*	$: $1 < @ $j . > $2		local domain
ifdef(`_NO_UUCP_', `dnl',
`R$* < @ localhost . UUCP > $*	$: $1 < @ $j . > $2		.UUCP domain')

# check for IPv6 domain literal (save quoted form)
R$* < @ [ IPv6 $- ] > $*	$: $2 $| $1 < @@ [ $(dequote $2 $) ] > $3	mark IPv6 addr
R$- $| $* < @@ $=w > $*		$: $2 < @ $j . > $4		self-literal
R$- $| $* < @@ [ $+ ] > $*	$@ $2 < @ [ IPv6 $1 ] > $4	canon IP addr

# check for IPv4 domain literal
R$* < @ [ $+ ] > $*		$: $1 < @@ [ $2 ] > $3		mark [a.b.c.d]
R$* < @@ $=w > $*		$: $1 < @ $j . > $3		self-literal
R$* < @@ $+ > $*		$@ $1 < @ $2 > $3		canon IP addr

ifdef(`_DOMAIN_TABLE_', `dnl
# look up domains in the domain table
R$* < @ $+ > $* 		$: $1 < @ $(domaintable $2 $) > $3', `dnl')

undivert(2)dnl LOCAL_RULE_3

ifdef(`_BITDOMAIN_TABLE_', `dnl
# handle BITNET mapping
R$* < @ $+ .BITNET > $*		$: $1 < @ $(bitdomain $2 $: $2.BITNET $) > $3', `dnl')

ifdef(`_UUDOMAIN_TABLE_', `dnl
# handle UUCP mapping
R$* < @ $+ .UUCP > $*		$: $1 < @ $(uudomain $2 $: $2.UUCP $) > $3', `dnl')

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

ifdef(`_NO_CANONIFY_', `dnl', `dnl
# try UUCP traffic as a local address
R$* < @ $+ . UUCP > $*		$: $1 < @ $[ $2 $] . UUCP . > $3
R$* < @ $+ . . UUCP . > $*	$@ $1 < @ $2 . > $3')
')')
# hostnames ending in class P are always canonical
R$* < @ $* $=P > $*		$: $1 < @ $2 $3 . > $4
dnl apply the next rule only for hostnames not in class P
dnl this even works for phrases in class P since . is in class P
dnl which daemon flags are set?
R$* < @ $* $~P > $*		$: $&{daemon_flags} $| $1 < @ $2 $3 > $4
dnl the other rules in this section only apply if the hostname
dnl does not end in class P hence no further checks are done here
dnl if this ever changes make sure the lookups are "protected" again!
ifdef(`_NO_CANONIFY_', `dnl
dnl do not canonify unless:
dnl domain ends in class {Canonify} (this does not work if the intersection
dnl	with class P is non-empty)
dnl or {daemon_flags} has c set
# pass to name server to make hostname canonical if in class {Canonify}
R$* $| $* < @ $* $={Canonify} > $*	$: $2 < @ $[ $3 $4 $] > $5
# pass to name server to make hostname canonical if requested
R$* c $* $| $* < @ $* > $*	$: $3 < @ $[ $4 $] > $5
dnl trailing dot? -> do not apply _CANONIFY_HOSTS_
R$* $| $* < @ $+ . > $*		$: $2 < @ $3 . > $4
# add a trailing dot to qualified hostnames so other rules will work
R$* $| $* < @ $+.$+ > $*	$: $2 < @ $3.$4 . > $5
ifdef(`_CANONIFY_HOSTS_', `dnl
dnl this should only apply to unqualified hostnames
dnl but if a valid character inside an unqualified hostname is an OperatorChar
dnl then $- does not work.
# lookup unqualified hostnames
R$* $| $* < @ $* > $*	$: $2 < @ $[ $3 $] > $4', `dnl')', `dnl
dnl _NO_CANONIFY_ is not set: canonify unless:
dnl {daemon_flags} contains CC (do not canonify)
R$* CC $* $| $*			$: $3
# pass to name server to make hostname canonical
R$* $| $* < @ $* > $*		$: $2 < @ $[ $3 $] > $4')
dnl remove {daemon_flags} for other cases
R$* $| $*			$: $2

# local host aliases and pseudo-domains are always canonical
R$* < @ $=w > $*		$: $1 < @ $2 . > $3
ifdef(`_MASQUERADE_ENTIRE_DOMAIN_',
`R$* < @ $* $=M > $*		$: $1 < @ $2 $3 . > $4',
`R$* < @ $=M > $*		$: $1 < @ $2 . > $3')
ifdef(`_VIRTUSER_TABLE_', `dnl
dnl virtual hosts are also canonical
ifdef(`_VIRTUSER_ENTIRE_DOMAIN_',
`R$* < @ $* $={VirtHost} > $* 	$: $1 < @ $2 $3 . > $4',
`R$* < @ $={VirtHost} > $* 	$: $1 < @ $2 . > $3')',
`dnl')
dnl remove superfluous dots (maybe repeatedly) which may have been added
dnl by one of the rules before
R$* < @ $* . . > $*		$1 < @ $2 . > $3


##################################################
###  Ruleset 4 -- Final Output Post-rewriting  ###
##################################################
Sfinal=4

R$* <@>			$@				handle <> and list:;

# strip trailing dot off possibly canonical name
R$* < @ $+ . > $*	$1 < @ $2 > $3

# eliminate internal code
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
R$+ % $=w @ $=w		$1 @ $2				u%host@host => u@host



##############################################################
###   Ruleset 97 -- recanonicalize and call ruleset zero   ###
###		   (used for recursive calls)		   ###
##############################################################

SRecurse=97
R$*			$: $>canonify $1
R$*			$@ $>parse $1


######################################
###   Ruleset 0 -- Parse Address   ###
######################################

Sparse=0

R$*			$: $>Parse0 $1		initial parsing
R<@>			$#_LOCAL_ $: <@>		special case error msgs
R$*			$: $>ParseLocal $1	handle local hacks
R$*			$: $>Parse1 $1		final parsing

#
#  Parse0 -- do initial syntax checking and eliminate local addresses.
#	This should either return with the (possibly modified) input
#	or return with a #error mailer.  It should not return with a
#	#mailer other than the #error mailer.
#

SParse0
R<@>			$@ <@>			special case error msgs
R$* : $* ; <@>		$#error $@ 5.1.3 $: "501 List:; syntax illegal for recipient addresses"
R@ <@ $* >		< @ $1 >		catch "@@host" bogosity
R<@ $+>			$#error $@ 5.1.3 $: "501 User address required"
R$*			$: <> $1
R<> $* < @ [ $+ ] > $*	$1 < @ [ $2 ] > $3
R<> $* <$* : $* > $*	$#error $@ 5.1.3 $: "501 Colon illegal in host name part"
R<> $*			$1
R$* < @ . $* > $*	$#error $@ 5.1.2 $: "501 Invalid host name"
R$* < @ $* .. $* > $*	$#error $@ 5.1.2 $: "501 Invalid host name"
dnl comma only allowed before @; this check is not complete
R$* , $~O $*		$#error $@ 5.1.2 $: "501 Invalid route address"

# now delete the local info -- note $=O to find characters that cause forwarding
R$* < @ > $*		$@ $>Parse0 $>canonify $1	user@ => user
R< @ $=w . > : $*	$@ $>Parse0 $>canonify $2	@here:... -> ...
R$- < @ $=w . >		$: $(dequote $1 $) < @ $2 . >	dequote "foo"@here
R< @ $+ >		$#error $@ 5.1.3 $: "501 User address required"
R$* $=O $* < @ $=w . >	$@ $>Parse0 $>canonify $1 $2 $3	...@here -> ...
R$- 			$: $(dequote $1 $) < @ *LOCAL* >	dequote "foo"
R< @ *LOCAL* >		$#error $@ 5.1.3 $: "501 User address required"
R$* $=O $* < @ *LOCAL* >
			$@ $>Parse0 $>canonify $1 $2 $3	...@*LOCAL* -> ...
R$* < @ *LOCAL* >	$: $1

#
#  Parse1 -- the bottom half of ruleset 0.
#

SParse1
ifdef(`_LDAP_ROUTING_', `dnl
# handle LDAP routing for hosts in $={LDAPRoute}
R$+ < @ $={LDAPRoute} . >	$: $>LDAPExpand <$1 < @ $2 . >> <$1 @ $2>',
`dnl')

ifdef(`_MAILER_smtp_',
`# handle numeric address spec
dnl there is no check whether this is really an IP number
R$* < @ [ $+ ] > $*	$: $>ParseLocal $1 < @ [ $2 ] > $3	numeric internet spec
R$* < @ [ $+ ] > $*	$1 < @ [ $2 ] : $S > $3		Add smart host to path
R$* < @ [ IPv6 $- ] : > $*
		$#_SMTP_ $@ [ $(dequote $2 $) ] $: $1 < @ [IPv6 $2 ] > $3	no smarthost: send
R$* < @ [ $+ ] : > $*	$#_SMTP_ $@ [$2] $: $1 < @ [$2] > $3	no smarthost: send
R$* < @ [ $+ ] : $- : $*> $*	$#$3 $@ $4 $: $1 < @ [$2] > $5	smarthost with mailer
R$* < @ [ $+ ] : $+ > $*	$#_SMTP_ $@ $3 $: $1 < @ [$2] > $4	smarthost without mailer',
	`dnl')

ifdef(`_VIRTUSER_TABLE_', `dnl
# handle virtual users
R$+			$: <!> $1		Mark for lookup
ifdef(`_VIRTUSER_ENTIRE_DOMAIN_',
`R<!> $+ < @ $* $={VirtHost} . > 	$: < $(virtuser $1 @ $2 $3 $@ $1 $: @ $) > $1 < @ $2 $3 . >',
`R<!> $+ < @ $={VirtHost} . > 	$: < $(virtuser $1 @ $2 $@ $1 $: @ $) > $1 < @ $2 . >')
R<!> $+ < @ $=w . > 	$: < $(virtuser $1 @ $2 $@ $1 $: @ $) > $1 < @ $2 . >
R<@> $+ + $* < @ $* . >
			$: < $(virtuser $1 + * @ $3 $@ $1 $@ $2 $: @ $) > $1 + $2 < @ $3 . >
R<@> $+ + $* < @ $* . >
			$: < $(virtuser $1 @ $3 $@ $1 $: @ $) > $1 + $2 < @ $3 . >
dnl try default entry: @domain
dnl +*@domain
R<@> $+ + $+ < @ $+ . >	$: < $(virtuser + * @ $3 $@ $1 $@ $2 $: @ $) > $1 + $2 < @ $3 . >
dnl @domain if +detail exists
R<@> $+ + $* < @ $+ . >	$: < $(virtuser @ $3 $@ $1 $@ $2 $: @ $) > $1 + $2 < @ $3 . >
dnl without +detail (or no match)
R<@> $+ < @ $+ . >	$: < $(virtuser @ $2 $@ $1 $: @ $) > $1 < @ $2 . >
R<@> $+			$: $1
R<!> $+			$: $1
R< error : $-.$-.$- : $+ > $* 	$#error $@ $1.$2.$3 $: $4
R< error : $- $+ > $* 	$#error $@ $(dequote $1 $) $: $2
R< $+ > $+ < @ $+ >	$: $>Recurse $1',
`dnl')

# short circuit local delivery so forwarded email works
ifdef(`_MAILER_usenet_', `dnl
R$+ . USENET < @ $=w . >	$#usenet $@ usenet $: $1	handle usenet specially', `dnl')


ifdef(`_STICKY_LOCAL_DOMAIN_',
`R$+ < @ $=w . >		$: < $H > $1 < @ $2 . >		first try hub
R< $+ > $+ < $+ >	$>MailerToTriple < $1 > $2 < $3 >	yep ....
dnl $H empty (but @$=w.)
R< > $+ + $* < $+ >	$#_LOCAL_ $: $1 + $2		plussed name?
R< > $+ < $+ >		$#_LOCAL_ $: @ $1			nope, local address',
`R$=L < @ $=w . >	$#_LOCAL_ $: @ $1			special local names
R$+ < @ $=w . >		$#_LOCAL_ $: $1			regular local name')

ifdef(`_MAILER_TABLE_', `dnl
# not local -- try mailer table lookup
R$* <@ $+ > $*		$: < $2 > $1 < @ $2 > $3	extract host name
R< $+ . > $*		$: < $1 > $2			strip trailing dot
R< $+ > $*		$: < $(mailertable $1 $) > $2	lookup
dnl it is $~[ instead of $- to avoid matches on IPv6 addresses
R< $~[ : $* > $* 	$>MailerToTriple < $1 : $2 > $3		check -- resolved?
R< $+ > $*		$: $>Mailertable <$1> $2		try domain',
`dnl')
undivert(4)dnl UUCP rules from `MAILER(uucp)'

ifdef(`_NO_UUCP_', `dnl',
`# resolve remotely connected UUCP links (if any)
ifdef(`_CLASS_V_',
`R$* < @ $=V . UUCP . > $*		$: $>MailerToTriple < $V > $1 <@$2.UUCP.> $3',
	`dnl')
ifdef(`_CLASS_W_',
`R$* < @ $=W . UUCP . > $*		$: $>MailerToTriple < $W > $1 <@$2.UUCP.> $3',
	`dnl')
ifdef(`_CLASS_X_',
`R$* < @ $=X . UUCP . > $*		$: $>MailerToTriple < $X > $1 <@$2.UUCP.> $3',
	`dnl')')

# resolve fake top level domains by forwarding to other hosts
ifdef(`BITNET_RELAY',
`R$*<@$+.BITNET.>$*	$: $>MailerToTriple < $B > $1 <@$2.BITNET.> $3	user@host.BITNET',
	`dnl')
ifdef(`DECNET_RELAY',
`R$*<@$+.DECNET.>$*	$: $>MailerToTriple < $C > $1 <@$2.DECNET.> $3	user@host.DECNET',
	`dnl')
ifdef(`_MAILER_pop_',
`R$+ < @ POP. >		$#pop $: $1			user@POP',
	`dnl')
ifdef(`_MAILER_fax_',
`R$+ < @ $+ .FAX. >	$#fax $@ $2 $: $1		user@host.FAX',
`ifdef(`FAX_RELAY',
`R$*<@$+.FAX.>$*		$: $>MailerToTriple < $F > $1 <@$2.FAX.> $3	user@host.FAX',
	`dnl')')

ifdef(`UUCP_RELAY',
`# forward non-local UUCP traffic to our UUCP relay
R$*<@$*.UUCP.>$*		$: $>MailerToTriple < $Y > $1 <@$2.UUCP.> $3	uucp mail',
`ifdef(`_MAILER_uucp_',
`# forward other UUCP traffic straight to UUCP
R$* < @ $+ .UUCP. > $*		$#_UUCP_ $@ $2 $: $1 < @ $2 .UUCP. > $3	user@host.UUCP',
	`dnl')')
ifdef(`_MAILER_usenet_', `
# addresses sent to net.group.USENET will get forwarded to a newsgroup
R$+ . USENET		$#usenet $@ usenet $: $1',
	`dnl')

ifdef(`_LOCAL_RULES_',
`# figure out what should stay in our local mail system
undivert(1)', `dnl')

# pass names that still have a host to a smarthost (if defined)
R$* < @ $* > $*		$: $>MailerToTriple < $S > $1 < @ $2 > $3	glue on smarthost name

# deal with other remote names
ifdef(`_MAILER_smtp_',
`R$* < @$* > $*		$#_SMTP_ $@ $2 $: $1 < @ $2 > $3	user@host.domain',
`R$* < @$* > $*		$#error $@ 5.1.2 $: "501 Unrecognized host name " $2')

# handle locally delivered names
R$=L			$#_LOCAL_ $: @ $1		special local names
R$+			$#_LOCAL_ $: $1			regular local names

###########################################################################
###   Ruleset 5 -- special rewriting after aliases have been expanded   ###
###########################################################################

SLocal_localaddr
Slocaladdr=5
R$+			$: $1 $| $>"Local_localaddr" $1
R$+ $| $#$*		$#$2
R$+ $| $*		$: $1

ifdef(`_FFR_5_', `
# Preserve host in a macro
R$+			$: $(macro {LocalAddrHost} $) $1
R$+ @ $+		$: $(macro {LocalAddrHost} $@ @ $2 $) $1')

ifdef(`_PRESERVE_LOCAL_PLUS_DETAIL_', `', `
# deal with plussed users so aliases work nicely
R$+ + *			$#_LOCAL_ $@ $&h $: $1`'ifdef(`_FFR_5_', ` $&{LocalAddrHost}')
R$+ + $*		$#_LOCAL_ $@ + $2 $: $1 + *`'ifdef(`_FFR_5_', ` $&{LocalAddrHost}')
')
# prepend an empty "forward host" on the front
R$+			$: <> $1

ifdef(`LUSER_RELAY', `dnl
# send unrecognized local users to a relay host
ifdef(`_PRESERVE_LOCAL_PLUS_DETAIL_', `
R< > $+ + $*		$: < ? $L > <+ $2> $(user $1 $)	look up user+
R< > $+			$: < ? $L > < > $(user $1 $)	look up user
R< ? $* > < $* > $+ <>	$: < > $3 $2			found; strip $L
R< ? $* > < $* > $+	$: < $1 > $3 $2			not found', `
R< > $+ 		$: < $L > $(user $1 $)		look up user
R< $* > $+ <>		$: < > $2			found; strip $L')',
`dnl')

# see if we have a relay or a hub
R< > $+			$: < $H > $1			try hub
R< > $+			$: < $R > $1			try relay
ifdef(`_PRESERVE_LOCAL_PLUS_DETAIL_', `
R< > $+			$@ $1', `
R< > $+			$: < > < $1 <> $&h >		nope, restore +detail
R< > < $+ <> + $* >	$: < > < $1 + $2 >		check whether +detail
R< > < $+ <> $* >	$: < > < $1 >			else discard
R< > < $+ + $* > $*	   < > < $1 > + $2 $3		find the user part
R< > < $+ > + $*	$#_LOCAL_ $@ $2 $: @ $1`'ifdef(`_FFR_5_', ` $&{LocalAddrHost}')		strip the extra +
R< > < $+ >		$@ $1				no +detail
R$+			$: $1 <> $&h			add +detail back in
R$+ <> + $*		$: $1 + $2			check whether +detail
R$+ <> $*		$: $1				else discard')
R< local : $* > $*	$: $>MailerToTriple < local : $1 > $2	no host extension
R< error : $* > $*	$: $>MailerToTriple < error : $1 > $2	no host extension
R< $- : $+ > $+		$: $>MailerToTriple < $1 : $2 > $3 < @ $2 >
R< $+ > $+		$@ $>MailerToTriple < $1 > $2 < @ $1 >

ifdef(`_MAILER_TABLE_', `dnl
###################################################################
###  Ruleset 90 -- try domain part of mailertable entry 	###
dnl input: LeftPartOfDomain <RightPartOfDomain> FullAddress
###################################################################

SMailertable=90
dnl shift and check
dnl %2 is not documented in cf/README
R$* <$- . $+ > $*	$: $1$2 < $(mailertable .$3 $@ $1$2 $@ $2 $) > $4
dnl it is $~[ instead of $- to avoid matches on IPv6 addresses
R$* <$~[ : $* > $*	$>MailerToTriple < $2 : $3 > $4		check -- resolved?
R$* < . $+ > $* 	$@ $>Mailertable $1 . <$2> $3		no -- strip & try again
dnl is $2 always empty?
R$* < $* > $*		$: < $(mailertable . $@ $1$2 $) > $3	try "."
R< $~[ : $* > $*	$>MailerToTriple < $1 : $2 > $3		"." found?
dnl return full address
R< $* > $*		$@ $2				no mailertable match',
`dnl')

###################################################################
###  Ruleset 95 -- canonify mailer:[user@]host syntax to triple	###
dnl input: in general: <[mailer:]host> lp<@domain>rest
dnl	<> address				-> address
dnl	<error:d.s.n:text>			-> error
dnl	<error:text>				-> error
dnl	<mailer:user@host> lp<@domain>rest	-> mailer host user
dnl	<mailer:host> address			-> mailer host address
dnl	<localdomain> address			-> address
dnl	<[IPv6 number]> address			-> relay number address
dnl	<host> address				-> relay host address
###################################################################

SMailerToTriple=95
R< > $*				$@ $1			strip off null relay
R< error : $-.$-.$- : $+ > $* 	$#error $@ $1.$2.$3 $: $4
R< error : $- $+ > $*		$#error $@ $(dequote $1 $) $: $2
R< local : $* > $*		$>CanonLocal < $1 > $2
R< $- : $+ @ $+ > $*<$*>$*	$# $1 $@ $3 $: $2<@$3>	use literal user
R< $- : $+ > $*			$# $1 $@ $2 $: $3	try qualified mailer
R< $=w > $*			$@ $2			delete local host
R< [ IPv6 $+ ] > $*		$#_RELAY_ $@ $(dequote $1 $) $: $2	use unqualified mailer
R< $+ > $*			$#_RELAY_ $@ $1 $: $2	use unqualified mailer

###################################################################
###  Ruleset CanonLocal -- canonify local: syntax		###
dnl input: <user> address
dnl <x> <@host> : rest			-> Recurse rest
dnl <x> p1 $=O p2 <@host>		-> Recurse p1 $=O p2
dnl <> user <@host> rest		-> local user@host user
dnl <> user				-> local user user
dnl <user@host> lp <@domain> rest	-> <user> lp <@host> [cont]
dnl <user> lp <@host> rest		-> local lp@host user
dnl <user> lp				-> local lp user
###################################################################

SCanonLocal
# strip local host from routed addresses
R< $* > < @ $+ > : $+		$@ $>Recurse $3
R< $* > $+ $=O $+ < @ $+ >	$@ $>Recurse $2 $3 $4

# strip trailing dot from any host name that may appear
R< $* > $* < @ $* . >		$: < $1 > $2 < @ $3 >

# handle local: syntax -- use old user, either with or without host
R< > $* < @ $* > $*		$#_LOCAL_ $@ $1@$2 $: $1
R< > $+				$#_LOCAL_ $@ $1    $: $1

# handle local:user@host syntax -- ignore host part
R< $+ @ $+ > $* < @ $* >	$: < $1 > $3 < @ $4 >

# handle local:user syntax
R< $+ > $* <@ $* > $*		$#_LOCAL_ $@ $2@$3 $: $1
R< $+ > $* 			$#_LOCAL_ $@ $2    $: $1

###################################################################
###  Ruleset 93 -- convert header names to masqueraded form	###
###################################################################

SMasqHdr=93

ifdef(`_GENERICS_TABLE_', `dnl
# handle generics database
ifdef(`_GENERICS_ENTIRE_DOMAIN_',
dnl if generics should be applied add a @ as mark
`R$+ < @ $* $=G . >	$: < $1@$2$3 > $1 < @ $2$3 . > @	mark',
`R$+ < @ $=G . >	$: < $1@$2 > $1 < @ $2 . > @	mark')
R$+ < @ *LOCAL* >	$: < $1@$j > $1 < @ *LOCAL* > @	mark
dnl workspace: either user<@domain> or <user@domain> user <@domain> @
dnl ignore the first case for now
dnl if it has the mark lookup full address
R< $+ > $+ < $* > @	$: < $(generics $1 $: @ $1 $) > $2 < $3 >
dnl workspace: ... or <match|@user@domain> user <@domain>
dnl no match, try user+detail@domain
R<@$+ + $* @ $+> $+ < @ $+ >
		$: < $(generics $1+*@$3 $@ $2 $:@$1 + $2@$3 $) >  $4 < @ $5 >
R<@$+ + $* @ $+> $+ < @ $+ >
		$: < $(generics $1@$3 $: $) > $4 < @ $5 >
dnl no match, remove mark
R<@$+ > $+ < @ $+ >	$: < > $2 < @ $3 >
dnl no match, try @domain for exceptions
R< > $+ < @ $+ . >	$: < $(generics @$2 $@ $1 $: $) > $1 < @ $2 . >
dnl workspace: ... or <match> user <@domain>
dnl no match, try local part
R< > $+ < @ $+ > 	$: < $(generics $1 $: $) > $1 < @ $2 >
R< > $+ + $* < @ $+ > 	$: < $(generics $1+* $@ $2 $: $) > $1 + $2 < @ $3 >
R< > $+ + $* < @ $+ > 	$: < $(generics $1 $: $) > $1 + $2 < @ $3 >
R< $* @ $* > $* < $* >	$@ $>canonify $1 @ $2		found qualified
R< $+ > $* < $* >	$: $>canonify $1 @ *LOCAL*	found unqualified
R< > $*			$: $1				not found',
`dnl')

# do not masquerade anything in class N
R$* < @ $* $=N . >	$@ $1 < @ $2 $3 . >

# special case the users that should be exposed
R$=E < @ *LOCAL* >	$@ $1 < @ $j . >		leave exposed
ifdef(`_MASQUERADE_ENTIRE_DOMAIN_',
`R$=E < @ $* $=M . >	$@ $1 < @ $2 $3 . >',
`R$=E < @ $=M . >	$@ $1 < @ $2 . >')
ifdef(`_LIMITED_MASQUERADE_', `dnl',
`R$=E < @ $=w . >	$@ $1 < @ $2 . >')

# handle domain-specific masquerading
ifdef(`_MASQUERADE_ENTIRE_DOMAIN_',
`R$* < @ $* $=M . > $*	$: $1 < @ $2 $3 . @ $M > $4	convert masqueraded doms',
`R$* < @ $=M . > $*	$: $1 < @ $2 . @ $M > $3	convert masqueraded doms')
ifdef(`_LIMITED_MASQUERADE_', `dnl',
`R$* < @ $=w . > $*	$: $1 < @ $2 . @ $M > $3')
R$* < @ *LOCAL* > $*	$: $1 < @ $j . @ $M > $2
R$* < @ $+ @ > $*	$: $1 < @ $2 > $3		$M is null
R$* < @ $+ @ $+ > $*	$: $1 < @ $3 . > $4		$M is not null

###################################################################
###  Ruleset 94 -- convert envelope names to masqueraded form	###
###################################################################

SMasqEnv=94
ifdef(`_MASQUERADE_ENVELOPE_',
`R$+			$@ $>MasqHdr $1',
`R$* < @ *LOCAL* > $*	$: $1 < @ $j . > $2')

###################################################################
###  Ruleset 98 -- local part of ruleset zero (can be null)	###
###################################################################

SParseLocal=98
undivert(3)dnl LOCAL_RULE_0

ifdef(`_LDAP_ROUTING_', `dnl
SLDAPExpand
# do the LDAP lookups
R<$+><$+>		$: <$(ldapmra $2 $: $)> <$(ldapmh $2 $: $)> <$1> <$2>

# if mailRoutingAddress and local or non-existant mailHost,
# return the new mailRoutingAddress
R< $+ > < $=w > < $+ > < $+ >	$@ $>Parse0 $>canonify $1
R< $+ > <  > < $+ > < $+ >	$@ $>Parse0 $>canonify $1

# if mailRoutingAddress and non-local mailHost,
# relay to mailHost with new mailRoutingAddress
R< $+ > < $+ > < $+ > < $+ >	$#_RELAY_ $@ $2 $: $>canonify $1

# if no mailRoutingAddress and local mailHost,
# return original address
R< > < $=w > <$+> <$+>		$@ $2

# if no mailRoutingAddress and non-local mailHost,
# relay to mailHost with original address
R< > < $+ > <$+> <$+>		$#_RELAY_ $@ $1 $: $2

# if no mailRoutingAddress and no mailHost,
# try @domain
R< > < > <$+> <$+ @ $+>		$@ $>LDAPExpand <$1> <@ $3>

# if no mailRoutingAddress and no mailHost and this was a domain attempt,
ifelse(_LDAP_ROUTING_, `_MUST_EXIST_', `dnl
# user does not exist
R< > < > <$+> <@ $+>		$#error $@ nouser $: "550 User unknown"',
`dnl
# return the original address
R< > < > <$+> <@ $+>		$@ $1')',
`dnl')

ifelse(substr(confDELIVERY_MODE,0,1), `d', `errprint(`WARNING: Antispam rules not available in deferred delivery mode.
')')
ifdef(`_ACCESS_TABLE_', `dnl
######################################################################
###  LookUpDomain -- search for domain in access database
###
###	Parameters:
###		<$1> -- key (domain name)
###		<$2> -- default (what to return if not found in db)
dnl			must not be empty
###		<$3> -- passthru (additional data passed unchanged through)
###		<$4> -- mark (must be <(!|+) single-token>)
###			! does lookup only with tag
###			+ does lookup with and without tag
dnl returns:		<default> <passthru>
dnl 			<result> <passthru>
######################################################################

SLookUpDomain
dnl remove IPv6 mark and dequote address
dnl it is a bit ugly because it is checked on each "iteration"
R<[IPv6 $-]> <$+> <$*> <$*>	$: <[$(dequote $1 $)]> <$2> <$3> <$4>
dnl workspace <key> <default> <passthru> <mark>
dnl lookup with tag (in front, no delimiter here)
R<$*> <$+> <$*> <$- $->		$: < $(access $5`'_TAG_DELIM_`'$1 $: ? $) > <$1> <$2> <$3> <$4 $5>
dnl workspace <result-of-lookup|?> <key> <default> <passthru> <mark>
ifdef(`_FFR_LOOKUPDOTDOMAIN', `dnl omit first component: lookup .rest
R<?> <$+.$+> <$+> <$*> <$- $->	$: < $(access $5`'_TAG_DELIM_`'.$2 $: ? $) > <$1.$2> <$3> <$4> <$5 $6>', `dnl')
dnl lookup without tag?
R<?> <$+> <$+> <$*> <+ $*>	$: < $(access $1 $: ? $) > <$1> <$2> <$3> <+ $4>
ifdef(`_FFR_LOOKUPDOTDOMAIN', `dnl omit first component: lookup .rest
R<?> <$+.$+> <$+> <$*> <+ $*>	$: < $(access .$2 $: ? $) > <$1.$2> <$3> <$4> <+ $5>', `dnl')
dnl lookup IP address (no check is done whether it is an IP number!)
R<?> <[$+.$-]> <$+> <$*> <$*>	$@ $>LookUpDomain <[$1]> <$3> <$4> <$5>
dnl lookup IPv6 address
R<?> <[$+:$-]> <$+> <$*> <$*>	$: $>LookUpDomain <[$1]> <$3> <$4> <$5>
dnl not found, but subdomain: try again
R<?> <$+.$+> <$+> <$*> <$*>	$@ $>LookUpDomain <$2> <$3> <$4> <$5>
dnl not found, no subdomain: return default
R<?> <$+> <$+> <$*> <$*>	$@ <$2> <$3>
dnl return result of lookup
R<$*> <$+> <$+> <$*> <$*>	$@ <$1> <$4>

######################################################################
###  LookUpAddress -- search for host address in access database
###
###	Parameters:
###		<$1> -- key (dot quadded host address)
###		<$2> -- default (what to return if not found in db)
dnl			must not be empty
###		<$3> -- passthru (additional data passed through)
###		<$4> -- mark (must be <(!|+) single-token>)
###			! does lookup only with tag
###			+ does lookup with and without tag
dnl	returns:	<default> <passthru>
dnl			<result> <passthru>
######################################################################

SLookUpAddress
dnl lookup with tag
R<$+> <$+> <$*> <$- $+>		$: < $(access $5`'_TAG_DELIM_`'$1 $: ? $) > <$1> <$2> <$3> <$4 $5>
dnl lookup without tag
R<?> <$+> <$+> <$*> <+ $+>	$: < $(access $1 $: ? $) > <$1> <$2> <$3> <+ $4>
dnl no match; IPv6: remove last part
R<?> <$+:$-> <$+> <$*> <$*>	$@ $>LookUpAddress <$1> <$3> <$4> <$5>
dnl no match; IPv4: remove last part
R<?> <$+.$-> <$+> <$*> <$*>	$@ $>LookUpAddress <$1> <$3> <$4> <$5>
dnl no match: return default
R<?> <$+> <$+> <$*> <$*>	$@ <$2> <$3>
dnl match: return result
R<$*> <$+> <$+> <$*> <$*>	$@ <$1> <$4>',
`dnl')

######################################################################
###  CanonAddr --	Convert an address into a standard form for
###			relay checking.  Route address syntax is
###			crudely converted into a %-hack address.
###
###	Parameters:
###		$1 -- full recipient address
###
###	Returns:
###		parsed address, not in source route form
dnl		user%host%host<@domain>
dnl		host!user<@domain>
######################################################################

SCanonAddr
R$*			$: $>Parse0 $>canonify $1	make domain canonical
ifdef(`_USE_DEPRECATED_ROUTE_ADDR_',`dnl
R< @ $+ > : $* @ $*	< @ $1 > : $2 % $3	change @ to % in src route
R$* < @ $+ > : $* : $*	$3 $1 < @ $2 > : $4	change to % hack.
R$* < @ $+ > : $*	$3 $1 < @ $2 >
dnl')

######################################################################
###  ParseRecipient --	Strip off hosts in $=R as well as possibly
###			$* $=m or the access database.
###			Check user portion for host separators.
###
###	Parameters:
###		$1 -- full recipient address
###
###	Returns:
###		parsed, non-local-relaying address
######################################################################

SParseRecipient
dnl mark and canonify address
R$*				$: <?> $>CanonAddr $1
dnl workspace: <?> localpart<@domain[.]>
R<?> $* < @ $* . >		<?> $1 < @ $2 >			strip trailing dots
dnl workspace: <?> localpart<@domain>
R<?> $- < @ $* >		$: <?> $(dequote $1 $) < @ $2 >	dequote local part

# if no $=O character, no host in the user portion, we are done
R<?> $* $=O $* < @ $* >		$: <NO> $1 $2 $3 < @ $4>
dnl no $=O in localpart: return
R<?> $*				$@ $1

dnl workspace: <?> localpart<@domain>, where localpart contains $=O
dnl mark everything which has an "authorized" domain with <RELAY>
ifdef(`_RELAY_ENTIRE_DOMAIN_', `dnl
# if we relay, check username portion for user%host so host can be checked also
R<NO> $* < @ $* $=m >		$: <RELAY> $1 < @ $2 $3 >', `dnl')

ifdef(`_RELAY_MX_SERVED_', `dnl
dnl do "we" ($=w) act as backup MX server for the destination domain?
R<NO> $* < @ $+ >		$: <MX> < : $(mxserved $2 $) : > < $1 < @$2 > >
R<MX> < : $* <TEMP> : > $*	$#error $@ 4.7.1 $: "450 Can not check MX records for recipient host " $1
dnl yes: mark it as <RELAY>
R<MX> < $* : $=w. : $* > < $+ >	$: <RELAY> $4
dnl no: put old <NO> mark back
R<MX> < : $* : > < $+ >		$: <NO> $2', `dnl')

dnl workspace: <(NO|RELAY)> localpart<@domain>, where localpart contains $=O
dnl if mark is <NO> then change it to <RELAY> if domain is "authorized"
ifdef(`_RELAY_HOSTS_ONLY_',
`R<NO> $* < @ $=R >		$: <RELAY> $1 < @ $2 >
ifdef(`_ACCESS_TABLE_', `dnl
R<NO> $* < @ $+ >		$: <$(access To:$2 $: NO $)> $1 < @ $2 >
R<NO> $* < @ $+ >		$: <$(access $2 $: NO $)> $1 < @ $2 >',`dnl')',
`R<NO> $* < @ $* $=R >		$: <RELAY> $1 < @ $2 $3 >
ifdef(`_ACCESS_TABLE_', `dnl
R<NO> $* < @ $+ >		$: $>LookUpDomain <$2> <NO> <$1 < @ $2 >> <+To>
R<$+> <$+>			$: <$1> $2',`dnl')')


R<RELAY> $* < @ $* >		$@ $>ParseRecipient $1
R<$-> $*			$@ $2


######################################################################
###  check_relay -- check hostname/address on SMTP startup
######################################################################

SLocal_check_relay
Scheck`'_U_`'relay
R$*			$: $1 $| $>"Local_check_relay" $1
R$* $| $* $| $#$*	$#$3
R$* $| $* $| $*		$@ $>"Basic_check_relay" $1 $| $2

SBasic_check_relay
# check for deferred delivery mode
R$*			$: < ${deliveryMode} > $1
R< d > $*		$@ deferred
R< $* > $*		$: $2

ifdef(`_ACCESS_TABLE_', `dnl
dnl workspace: {client_name} $| {client_addr}
R$+ $| $+		$: $>LookUpDomain < $1 > <?> < $2 > <+Connect>
dnl workspace: <result-of-lookup> <{client_addr}>
R<?> <$+>		$: $>LookUpAddress < $1 > <?> < $1 > <+Connect>	no: another lookup
dnl workspace: <result-of-lookup> <{client_addr}>
R<?> < $+ >		$: $1					found nothing
dnl workspace: <result-of-lookup> <{client_addr}>
dnl or {client_addr}
R<$={Accept}> < $* >	$@ $1				return value of lookup
R<REJECT> $*		$#error ifdef(`confREJECT_MSG', `$: "confREJECT_MSG"', `$@ 5.7.1 $: "550 Access denied"')
R<DISCARD> $*		$#discard $: discard
dnl error tag
R<ERROR:$-.$-.$-:$+> <$*>	$#error $@ $1.$2.$3 $: $4
R<ERROR:$+> <$*>		$#error $: $1
dnl generic error from access map
R<$+> <$*>		$#error $: $1', `dnl')

ifdef(`_RBL_',`dnl
# DNS based IP address spam list
R$*			$: $&{client_addr}
R::ffff:$-.$-.$-.$-	$: <?> $(host $4.$3.$2.$1._RBL_. $: OK $)
R$-.$-.$-.$-		$: <?> $(host $4.$3.$2.$1._RBL_. $: OK $)
R<?>OK			$: OKSOFAR
R<?>$+			$#error $@ 5.7.1 $: "550 Mail from " $&{client_addr} " refused by blackhole site _RBL_"',
`dnl')
undivert(8)

######################################################################
###  check_mail -- check SMTP ``MAIL FROM:'' command argument
######################################################################

SLocal_check_mail
Scheck`'_U_`'mail
R$*			$: $1 $| $>"Local_check_mail" $1
R$* $| $#$*		$#$2
R$* $| $*		$@ $>"Basic_check_mail" $1

SBasic_check_mail
# check for deferred delivery mode
R$*			$: < ${deliveryMode} > $1
R< d > $*		$@ deferred
R< $* > $*		$: $2

# authenticated?
dnl done first: we can require authentication for every mail transaction
dnl workspace: address as given by MAIL FROM: (sender)
R$*			$: $1 $| $>"tls_client" $&{verify} $| MAIL
R$* $| $#$+		$#$2
dnl undo damage: remove result of tls_client call
R$* $| $*		$: $1

dnl workspace: address as given by MAIL FROM:
R<>			$@ <OK>			we MUST accept <> (RFC 1123)
ifdef(`_ACCEPT_UNQUALIFIED_SENDERS_',`dnl',`dnl
dnl do some additional checks
dnl no user@host
dnl no user@localhost (if nonlocal sender)
dnl this is a pretty simple canonification, it will not catch every case
dnl just make sure the address has <> around it (which is required by
dnl the RFC anyway, maybe we should complain if they are missing...)
dnl dirty trick: if it is user@host, just add a dot: user@host. this will
dnl not be modified by host lookups.
R$+			$: <?> $1
R<?><$+>		$: <@> <$1>
R<?>$+			$: <@> <$1>
dnl workspace: <@> <address>
dnl prepend daemon_flags
R$*			$: $&{daemon_flags} $| $1
dnl workspace: ${daemon_flags} $| <@> <address>
dnl do not allow these at all or only from local systems?
R$* f $* $| <@> < $* @ $- >	$: < ? $&{client_name} > < $3 @ $4 >
dnl accept unqualified sender: change mark to avoid test
R$* u $* $| <@> < $* >	$: <?> < $3 >
dnl workspace: ${daemon_flags} $| <@> <address>
dnl        or:                    <? ${client_name} > <address>
dnl        or:                    <?> <address>
dnl remove daemon_flags
R$* $| $*		$: $2
# handle case of @localhost on address
R<@> < $* @ localhost >	$: < ? $&{client_name} > < $1 @ localhost >
R<@> < $* @ [127.0.0.1] >
			$: < ? $&{client_name} > < $1 @ [127.0.0.1] >
R<@> < $* @ localhost.$m >
			$: < ? $&{client_name} > < $1 @ localhost.$m >
ifdef(`_NO_UUCP_', `dnl',
`R<@> < $* @ localhost.UUCP >
			$: < ? $&{client_name} > < $1 @ localhost.UUCP >')
dnl workspace: < ? $&{client_name} > <user@localhost|host>
dnl	or:    <@> <address>
dnl	or:    <?> <address>	(thanks to u in ${daemon_flags})
R<@> $*			$: $1			no localhost as domain
dnl workspace: < ? $&{client_name} > <user@localhost|host>
dnl	or:    <address>
dnl	or:    <?> <address>	(thanks to u in ${daemon_flags})
R<? $=w> $*		$: $2			local client: ok
R<? $+> <$+>		$#error $@ 5.5.4 $: "501 Real domain name required for sender address"
dnl remove <?> (happens only if ${client_name} == "" or u in ${daemon_flags})
R<?> $*			$: $1')
dnl workspace: address (or <address>)
R$*			$: <?> $>CanonAddr $1		canonify sender address and mark it
dnl workspace: <?> CanonicalAddress (i.e. address in canonical form localpart<@host>)
dnl there is nothing behind the <@host> so no trailing $* needed
R<?> $* < @ $+ . >	<?> $1 < @ $2 >			strip trailing dots
# handle non-DNS hostnames (*.bitnet, *.decnet, *.uucp, etc)
R<?> $* < @ $* $=P >	$: <OK> $1 < @ $2 $3 >
dnl workspace <mark> CanonicalAddress	where mark is ? or OK
ifdef(`_ACCEPT_UNRESOLVABLE_DOMAINS_',
`R<?> $* < @ $+ >	$: <OK> $1 < @ $2 >		... unresolvable OK',
`R<?> $* < @ $+ >	$: <? $(resolve $2 $: $2 <PERM> $) > $1 < @ $2 >
R<? $* <$->> $* < @ $+ >
			$: <$2> $3 < @ $4 >')
dnl workspace <mark> CanonicalAddress	where mark is ?, OK, PERM, TEMP
dnl mark is ? iff the address is user (wo @domain)

ifdef(`_ACCESS_TABLE_', `dnl
# check sender address: user@address, user@, address
dnl should we remove +ext from user?
dnl workspace: <mark> CanonicalAddress where mark is: ?, OK, PERM, TEMP
R<$+> $+ < @ $* >	$: @<$1> <$2 < @ $3 >> $| <F:$2@$3> <U:$2@> <H:$3>
R<$+> $+		$: @<$1> <$2> $| <U:$2@>
dnl workspace: @<mark> <CanonicalAddress> $| <@type:address> ....
dnl $| is used as delimiter, otherwise false matches may occur: <user<@domain>>
dnl will only return user<@domain when "reversing" the args
R@ <$+> <$*> $| <$+>	$: <@> <$1> <$2> $| $>SearchList <+From> $| <$3> <>
dnl workspace: <@><mark> <CanonicalAddress> $| <result>
R<@> <$+> <$*> $| <$*>	$: <$3> <$1> <$2>		reverse result
dnl workspace: <result> <mark> <CanonicalAddress>
# retransform for further use
dnl required form:
dnl <ResultOfLookup|mark> CanonicalAddress
R<?> <$+> <$*>		$: <$1> $2	no match
R<$+> <$+> <$*>		$: <$1> $3	relevant result, keep it', `dnl')
dnl workspace <ResultOfLookup|mark> CanonicalAddress
dnl mark is ? iff the address is user (wo @domain)

ifdef(`_ACCEPT_UNQUALIFIED_SENDERS_',`dnl',`dnl
# handle case of no @domain on address
dnl prepend daemon_flags
R<?> $*			$: $&{daemon_flags} $| <?> $1
dnl accept unqualified sender: change mark to avoid test
R$* u $* $| <?> $*	$: <OK> $3
dnl remove daemon_flags
R$* $| $*		$: $2
R<?> $*			$: < ? $&{client_name} > $1
R<?> $*			$@ <OK>				...local unqualed ok
R<? $+> $*		$#error $@ 5.5.4 $: "501 Domain name required for sender address " $&f
							...remote is not')
# check results
R<?> $*			$: @ $1		mark address: nothing known about it
R<OK> $*		$@ <OK>
R<TEMP> $*		$#error $@ 4.1.8 $: "451 Domain of sender address " $&f " does not resolve"
R<PERM> $*		$#error $@ 5.1.8 $: "501 Domain of sender address " $&f " does not exist"
ifdef(`_ACCESS_TABLE_', `dnl
R<$={Accept}> $*	$# $1
R<DISCARD> $*		$#discard $: discard
R<REJECT> $*		$#error ifdef(`confREJECT_MSG', `$: "confREJECT_MSG"', `$@ 5.7.1 $: "550 Access denied"')
dnl error tag
R<ERROR:$-.$-.$-:$+> $*		$#error $@ $1.$2.$3 $: $4
R<ERROR:$+> $*		$#error $: $1
dnl generic error from access map
R<$+> $*		$#error $: $1		error from access db',
`dnl')

######################################################################
###  check_rcpt -- check SMTP ``RCPT TO:'' command argument
######################################################################

SLocal_check_rcpt
Scheck`'_U_`'rcpt
R$*			$: $1 $| $>"Local_check_rcpt" $1
R$* $| $#$*		$#$2
R$* $| $*		$@ $>"Basic_check_rcpt" $1

SBasic_check_rcpt
# check for deferred delivery mode
R$*			$: < ${deliveryMode} > $1
R< d > $*		$@ deferred
R< $* > $*		$: $2

ifdef(`_REQUIRE_QUAL_RCPT_', `dnl
# require qualified recipient?
R$+			$: <?> $1
R<?><$+>		$: <@> <$1>
R<?>$+			$: <@> <$1>
dnl prepend daemon_flags
R$*			$: $&{daemon_flags} $| $1
dnl workspace: ${daemon_flags} $| <@> <address>
dnl do not allow these at all or only from local systems?
R$* r $* $| <@> < $* @ $- >	$: < ? $&{client_name} > < $3 @ $4 >
R<?> < $* >		$: <$1>
R<? $=w> < $* >		$: <$1>
R<? $+> <$+>		$#error $@ 5.5.4 $: "553 Domain name required"
dnl remove daemon_flags for other cases
R$* $| <@> $*		$: $2', `dnl')

ifdef(`_LOOSE_RELAY_CHECK_',`dnl
R$*			$: $>CanonAddr $1
R$* < @ $* . >		$1 < @ $2 >			strip trailing dots',
`R$*			$: $>ParseRecipient $1		strip relayable hosts')

ifdef(`_BESTMX_IS_LOCAL_',`dnl
ifelse(_BESTMX_IS_LOCAL_, `', `dnl
# unlimited bestmx
R$* < @ $* > $*			$: $1 < @ $2 @@ $(bestmx $2 $) > $3',
`dnl
# limit bestmx to $=B
R$* < @ $* $=B > $*		$: $1 < @ $2 $3 @@ $(bestmx $2 $3 $) > $4')
R$* $=O $* < @ $* @@ $=w . > $*	$@ $>"Basic_check_rcpt" $1 $2 $3
R$* < @ $* @@ $=w . > $*	$: $1 < @ $3 > $4
R$* < @ $* @@ $* > $*		$: $1 < @ $2 > $4')

ifdef(`_BLACKLIST_RCPT_',`dnl
ifdef(`_ACCESS_TABLE_', `dnl
# blacklist local users or any host from receiving mail
R$*			$: <?> $1
dnl user is now tagged with @ to be consistent with check_mail
dnl and to distinguish users from hosts (com would be host, com@ would be user)
R<?> $+ < @ $=w >	$: <> <$1 < @ $2 >> $| <F:$1@$2> <U:$1@> <H:$2>
R<?> $+ < @ $* >	$: <> <$1 < @ $2 >> $| <F:$1@$2> <H:$2>
R<?> $+			$: <> <$1> $| <U:$1@>
dnl $| is used as delimiter, otherwise false matches may occur: <user<@domain>>
dnl will only return user<@domain when "reversing" the args
R<> <$*> $| <$+>	$: <@> <$1> $| $>SearchList <+To> $| <$2> <>
R<@> <$*> $| <$*>	$: <$2> <$1>		reverse result
R<?> <$*>		$: @ $1		mark address as no match
R<$={Accept}> <$*>	$: @ $2		mark address as no match
ifdef(`_DELAY_CHECKS_',`dnl
dnl we have to filter these because otherwise they would be interpreted
dnl as generic error message...
dnl error messages should be "tagged" by prefixing them with error: !
dnl that would make a lot of things easier.
dnl maybe we should stop checks already here (if SPAM_xyx)?
R<$={SpamTag}> <$*>	$: @ $2		mark address as no match')
R<REJECT> $*		$#error $@ 5.2.1 $: "550 Mailbox disabled for this recipient"
R<DISCARD> $*		$#discard $: discard
dnl error tag
R<ERROR:$-.$-.$-:$+> $*		$#error $@ $1.$2.$3 $: $4
R<ERROR:$+> $*		$#error $: $1
dnl generic error from access map
R<$+> $*		$#error $: $1		error from access db
R@ $*			$1		remove mark', `dnl')', `dnl')

ifdef(`_PROMISCUOUS_RELAY_', `divert(-1)')
# authenticated?
dnl do this unconditionally? this requires to manage CAs carefully
dnl just because someone has a CERT signed by a "trusted" CA
dnl does not mean we want to allow relaying for her,
dnl either use a subroutine or provide something more sophisticated
dnl this could for example check the DN (maybe an access map lookup)
R$*		$: $1 $| $>RelayAuth $1 $| $&{verify}	client authenticated?
R$* $| $# $+		$# $2				error/ok?
R$* $| $*		$: $1				no

# authenticated by a trusted mechanism?
R$*			$: $1 $| $&{auth_type}
dnl empty ${auth_type}?
R$* $|			$: $1
dnl mechanism ${auth_type} accepted?
dnl use $# to override further tests (delay_checks): see check_rcpt below
R$* $| $={TrustAuthMech}	$# RELAYAUTH
dnl undo addition of ${auth_type}
R$* $| $*		$: $1
dnl workspace: localpart<@domain>
ifelse(defn(`_NO_UUCP_'), `r',
`R$* ! $* < @ $* >	$: <REMOTE> $2 < @ BANG_PATH >', `dnl')
# anything terminating locally is ok
ifdef(`_RELAY_ENTIRE_DOMAIN_', `dnl
R$+ < @ $* $=m >	$@ RELAYTO', `dnl')
R$+ < @ $=w >		$@ RELAYTO
ifdef(`_RELAY_HOSTS_ONLY_',
`R$+ < @ $=R >		$@ RELAYTO
ifdef(`_ACCESS_TABLE_', `dnl
R$+ < @ $+ >		$: <$(access To:$2 $: ? $)> <$1 < @ $2 >>
dnl workspace: <Result-of-lookup | ?> <localpart<@domain>>
R<?> <$+ < @ $+ >>	$: <$(access $2 $: ? $)> <$1 < @ $2 >>',`dnl')',
`R$+ < @ $* $=R >	$@ RELAYTO
ifdef(`_ACCESS_TABLE_', `dnl
R$+ < @ $+ >		$: $>LookUpDomain <$2> <?> <$1 < @ $2 >> <+To>',`dnl')')
ifdef(`_ACCESS_TABLE_', `dnl
dnl workspace: <Result-of-lookup | ?> <localpart<@domain>>
R<RELAY> $*		$@ RELAYTO
R<$*> <$*>		$: $2',`dnl')


ifdef(`_RELAY_MX_SERVED_', `dnl
# allow relaying for hosts which we MX serve
R$+ < @ $+ >		$: < : $(mxserved $2 $) : > $1 < @ $2 >
dnl this must not necessarily happen if the client is checked first...
R< : $* <TEMP> : > $*	$#error $@ 4.7.1 $: "450 Can not check MX records for recipient host " $1
R<$* : $=w . : $*> $*	$@ RELAYTO
R< : $* : > $*		$: $2',
`dnl')

# check for local user (i.e. unqualified address)
R$*			$: <?> $1
R<?> $* < @ $+ >	$: <REMOTE> $1 < @ $2 >
# local user is ok
dnl is it really? the standard requires user@domain, not just user
dnl but we should accept it anyway (maybe making it an option:
dnl RequireFQDN ?)
dnl postmaster must be accepted without domain (DRUMS)
ifdef(`_REQUIRE_QUAL_RCPT_', `dnl
R<?> postmaster		$@ TOPOSTMASTER
# require qualified recipient?
dnl prepend daemon_flags
R<?> $+			$: $&{daemon_flags} $| <?> $1
dnl workspace: ${daemon_flags} $| <?> localpart
dnl do not allow these at all or only from local systems?
dnl r flag? add client_name
R$* r $* $| <?> $+	$: < ? $&{client_name} > <?> $3
dnl no r flag: relay to local user (only local part)
# no qualified recipient required
R$* $| <?> $+		$@ RELAYTOLOCAL
dnl client_name is empty
R<?> <?> $+		$@ RELAYTOLOCAL
dnl client_name is local
R<? $=w> <?> $+		$@ RELAYTOLOCAL
dnl client_name is not local
R<? $+> $+		$#error $@ 5.5.4 $: "553 Domain name required"', `dnl
dnl no qualified recipient required
R<?> $+			$@ RELAYTOLOCAL')
dnl it is a remote user: remove mark and then check client
R<$+> $*		$: $2
dnl currently the recipient address is not used below

# anything originating locally is ok
# check IP address
R$*			$: $&{client_addr}
R$@			$@ RELAYFROM		originated locally
R0			$@ RELAYFROM		originated locally
R$=R $*			$@ RELAYFROM		relayable IP address
ifdef(`_ACCESS_TABLE_', `dnl
R$*			$: $>LookUpAddress <$1> <?> <$1> <+Connect>
R<RELAY> $* 		$@ RELAYFROM		relayable IP address
R<$*> <$*>		$: $2', `dnl')
R$*			$: [ $1 ]		put brackets around it...
R$=w			$@ RELAYFROM		... and see if it is local

ifdef(`_RELAY_DB_FROM_', `define(`_RELAY_MAIL_FROM_', `1')')dnl
ifdef(`_RELAY_LOCAL_FROM_', `define(`_RELAY_MAIL_FROM_', `1')')dnl
ifdef(`_RELAY_MAIL_FROM_', `dnl
dnl input: {client_addr} or something "broken"
dnl just throw the input away; we do not need it.
# check whether FROM is allowed to use system as relay
R$*			$: <?> $>CanonAddr $&f
ifdef(`_RELAY_LOCAL_FROM_', `dnl
# check whether local FROM is ok
R<?> $+ < @ $=w . >	$@ RELAYFROMMAIL	FROM local', `dnl')
ifdef(`_RELAY_DB_FROM_', `dnl
R<?> $+ < @ $+ . >	<?> $1 < @ $2 >		remove trailing dot
R<?> $+ < @ $+ >	$: $1 < @ $2 > $| $>SearchList <! From> $| <F:$1@$2> ifdef(`_RELAY_DB_FROM_DOMAIN_', `<H:$2>') <>
R$* <RELAY>		$@ RELAYFROMMAIL	RELAY FROM sender ok', `dnl
ifdef(`_RELAY_DB_FROM_DOMAIN_', `errprint(`*** ERROR: _RELAY_DB_FROM_DOMAIN_ requires _RELAY_DB_FROM_
')',
`dnl')
dnl')', `dnl')

# check client name: first: did it resolve?
dnl input: ignored
R$*			$: < $&{client_resolve} >
R<TEMP>			$#error $@ 4.7.1 $: "450 Relaying temporarily denied. Cannot resolve PTR record for " $&{client_addr}
R<FORGED>		$#error $@ 5.7.1 $: "550 Relaying denied. IP name possibly forged " $&{client_name}
R<FAIL>			$#error $@ 5.7.1 $: "550 Relaying denied. IP name lookup failed " $&{client_name}
dnl ${client_resolve} should be OK, so go ahead
R$*			$: <?> $&{client_name}
# pass to name server to make hostname canonical
R<?> $* $~P 		$:<?>  $[ $1 $2 $]
R$* .			$1			strip trailing dots
dnl should not be necessary since it has been done for client_addr already
R<?>			$@ RELAYFROM
ifdef(`_RELAY_ENTIRE_DOMAIN_', `dnl
R<?> $* $=m		$@ RELAYFROM', `dnl')
R<?> $=w		$@ RELAYFROM
ifdef(`_RELAY_HOSTS_ONLY_',
`R<?> $=R		$@ RELAYFROM
ifdef(`_ACCESS_TABLE_', `dnl
R<?> $*			$: <$(access Connect:$1 $: ? $)> <$1>
R<?> <$*>		$: <$(access $1 $: ? $)> <$1>',`dnl')',
`R<?> $* $=R			$@ RELAYFROM
ifdef(`_ACCESS_TABLE_', `dnl
R<?> $*			$: $>LookUpDomain <$1> <?> <$1> <+Connect>',`dnl')')
ifdef(`_ACCESS_TABLE_', `dnl
R<RELAY> $*		$@ RELAYFROM
R<$*> <$*>		$: $2',`dnl')

# anything else is bogus
R$*			$#error $@ 5.7.1 $: confRELAY_MSG
divert(0)
ifdef(`_DELAY_CHECKS_',`dnl
# turn a canonical address in the form user<@domain>
# qualify unqual. addresses with $j
dnl it might have been only user (without <@domain>)
SFullAddr
R$* <@ $+ . >		$1 <@ $2 >
R$* <@ $* >		$@ $1 <@ $2 >
R$+			$@ $1 <@ $j >

# call all necessary rulesets
Scheck_rcpt
dnl this test should be in the Basic_check_rcpt ruleset
dnl which is the correct DSN code?
# R$@			$#error $@ 5.1.3 $: "553 Recipient address required"
R$+			$: $1 $| $>checkrcpt $1
dnl now we can simply stop checks by returning "$# xyz" instead of just "ok"
R$+ $| $#$*		$#$2
R$+ $| $*		$: <?> $>FullAddr $>CanonAddr $1
ifdef(`_SPAM_FH_',
`dnl lookup user@ and user@address
ifdef(`_ACCESS_TABLE_', `',
`errprint(`*** ERROR: FEATURE(`delay_checks', `argument') requires FEATURE(`access_db')
')')dnl
dnl one of the next two rules is supposed to match
dnl this code has been copied from BLACKLIST... etc
dnl and simplified by omitting some < >.
R<?> $+ < @ $=w >	$: <> $1 < @ $2 > $| <F: $1@$2 > <U: $1@>
R<?> $+ < @ $* >	$: <> $1 < @ $2 > $| <F: $1@$2 >
dnl R<?>		$@ something_is_very_wrong_here
# lookup the addresses only with To tag
R<> $* $| <$+>		$: <@> $1 $| $>SearchList <!To> $| <$2> <>
R<@> $* $| $*		$: $2 $1		reverse result
dnl', `dnl')
ifdef(`_SPAM_FRIEND_',
`# is the recipient a spam friend?
ifdef(`_SPAM_HATER_',
	`errprint(`*** ERROR: define either SpamHater or SpamFriend
')', `dnl')
R<SPAMFRIEND> $+	$@ SPAMFRIEND
R<$*> $+		$: $2',
`dnl')
ifdef(`_SPAM_HATER_',
`# is the recipient no spam hater?
R<SPAMHATER> $+		$: $1			spam hater: continue checks
R<$*> $+		$@ NOSPAMHATER		everyone else: stop
dnl',`dnl')
dnl run further checks: check_mail
dnl should we "clean up" $&f?
R$*			$: $1 $| $>checkmail <$&f>
R$* $| $#$*		$#$2
dnl run further checks: check_relay
R$*			$: $1 $| $>checkrelay $&{client_name} $| $&{client_addr}
R$* $| $#$*		$#$2
R$* $| $*		$: $1
', `dnl')
ifdef(`_ACCESS_TABLE_', `dnl
######################################################################
###  SearchList: search a list of items in the access map
###	Parameters:
###		<exact tag> $| <mark:address> <mark:address> ... <>
dnl	maybe we should have a @ (again) in front of the mark to
dnl	avoid errorneous matches (with error messages?)
dnl	if we can make sure that tag is always a single token
dnl	then we can omit the delimiter $|, otherwise we need it
dnl	to avoid errorneous matchs (first rule: H: if there
dnl	is that mark somewhere in the list, it will be taken).
dnl	moreover, we can do some tricks to enforce lookup with
dnl	the tag only, e.g.:
###	where "exact" is either "+" or "!":
###	<+ TAG>	lookup with and w/o tag
###	<! TAG>	lookup with tag
dnl	Warning: + and ! should be in OperatorChars (otherwise there must be
dnl		a blank between them and the tag.
###	possible values for "mark" are:
###		H: recursive host lookup (LookUpDomain)
dnl		A: recursive address lookup (LookUpAddress) [not yet required]
###		E: exact lookup, no modifications
###		F: full lookup, try user+ext@domain and user@domain
###		U: user lookup, try user+ext and user (input must have trailing @)
###	return: <RHS of lookup> or <?> (not found)
######################################################################

# class with valid marks for SearchList
dnl if A is activated: add it
C{src}E F H U
SSearchList
# mark H: lookup domain
R<$+> $| <H:$+> <$*>		$: <$1> $| <@> $>LookUpDomain <$2> <?> <$3> <$1>
R<$+> $| <@> <$+> <$*>		$: <$1> $| <$2> <$3>
dnl A: NOT YET REQUIRED
dnl R<$+> $| <A:$+> <$*>	$: <$1> $| <@> $>LookUpAddress <$2> <?> <$3> <$1>
dnl R<$+> $| <@> <$+> <$*>	$: <$1> $| <$2> <$3>
dnl lookup of the item with tag
dnl this applies to F: U: E:
R<$- $-> $| <$={src}:$+> <$*>	$: <$1 $2> $| <$(access $2`'_TAG_DELIM_`'$4 $: $3:$4 $)> <$5>
dnl no match, try without tag
R<+ $-> $| <$={src}:$+> <$*>	$: <+ $1> $| <$(access $3 $: $2:$3 $)> <$4>
dnl do we really have to distinguish these cases?
dnl probably yes, there might be a + in the domain part (is that allowed?)
dnl user+detail lookups: should it be:
dnl user+detail, user+*, user; just like aliases?
R<$- $-> $| <F:$* + $*@$+> <$*>	$: <$1 $2> $| <$(access $2`'_TAG_DELIM_`'$3@$5 $: F:$3 + $4@$5$)> <$6>
R<+ $-> $| <F:$* + $*@$+> <$*>	$: <+ $1> $| <$(access $2@$4 $: F:$2 + $3@$4$)> <$5>
dnl user lookups are always with trailing @
dnl do not remove the @ from the lookup:
dnl it is part of the +detail@ which is omitted for the lookup
R<$- $-> $| <U:$* + $*> <$*>	$: <$1 $2> $| <$(access $2`'_TAG_DELIM_`'$3@ $: U:$3 + $4$)> <$5>
dnl no match, try without tag
R<+ $-> $| <U:$* + $*> <$*>	$: <+ $1> $| <$(access $2@ $: U:$2 + $3$)> <$4>
dnl no match, try rest of list
R<$+> $| <$={src}:$+> <$+>	$@ $>SearchList <$1> $| <$4>
dnl no match, list empty: return failure
R<$+> $| <$={src}:$+> <>	$@ <?>
dnl got result, return it
R<$+> $| <$+> <$*>		$@ <$2>
dnl return result from recursive invocation
R<$+> $| <$+>			$@ <$2>', `dnl')

# is user trusted to authenticate as someone else?
dnl AUTH= parameter from MAIL command
Strust_auth
R$*			$: $&{auth_type} $| $1
# required by RFC 2554 section 4.
R$@ $| $*		$#error $@ 5.7.1 $: "550 not authenticated"
dnl seems to be useful...
R$* $| $&{auth_authen}		$@ identical
R$* $| <$&{auth_authen}>	$@ identical
dnl call user supplied code
R$* $| $*		$: $1 $| $>"Local_trust_auth" $1
R$* $| $#$*		$#$2
dnl default: error
R$*			$#error $@ 5.7.1 $: "550 " $&{auth_authen} " not allowed to act as " $&{auth_author}

dnl empty ruleset definition so it can be called
SLocal_trust_auth

ifdef(`_FFR_TLS_O_T', `dnl
Soffer_tls
R$*		$: $>LookUpDomain <$&{client_name}> <?> <> <! TLS_OFF_TAG>
R<?>$*		$: $>LookUpAddress <$&{client_addr}> <?> <> <! TLS_OFF_TAG>
R<?>$*		$: <$(access TLS_OFF_TAG: $: ? $)>
R<?>$*		$@ OK
R<NO> <>	$#error $@ 5.7.1 $: "550 do not offer TLS for " $&{client_name} " ["$&{client_addr}"]"

Stry_tls
R$*		$: $>LookUpDomain <$&{server_name}> <?> <> <! TLS_TRY_TAG>
R<?>$*		$: $>LookUpAddress <$&{server_addr}> <?> <> <! TLS_TRY_TAG>
R<?>$*		$: <$(access TLS_TRY_TAG: $: ? $)>
R<?>$*		$@ OK
R<NO> <>	$#error $@ 5.7.1 $: "550 do not try TLS with " $&{server_name} " ["$&{server_addr}"]"
')dnl

# is connection with client "good" enough? (done in server)
# input: ${verify} $| (MAIL|STARTTLS)
dnl MAIL: called from check_mail
dnl STARTTLS: called from smtp() after STARTTLS has been accepted
Stls_client
ifdef(`_ACCESS_TABLE_', `dnl
dnl ignore second arg for now
dnl maybe use it to distinguish permanent/temporary error?
dnl if MAIL: permanent (STARTTLS has not been offered)
dnl if STARTTLS: temporary (offered but maybe failed)
R$* $| $*	$: $1 $| $>LookUpDomain <$&{client_name}> <?> <> <! TLS_CLT_TAG>
R$* $| <?>$*	$: $1 $| $>LookUpAddress <$&{client_addr}> <?> <> <! TLS_CLT_TAG>
dnl do a default lookup: just TLS_CLT_TAG
R$* $| <?>$*	$: $1 $| <$(access TLS_CLT_TAG`'_TAG_DELIM_ $: ? $)>
R$*		$@ $>"tls_connection" $1', `dnl
R$* $| $*	$@ $>"tls_connection" $1')

# is connection with server "good" enough? (done in client)
dnl i.e. has the server been authenticated and is encryption active?
dnl called from deliver() after STARTTLS command
# input: ${verify}
Stls_server
ifdef(`_ACCESS_TABLE_', `dnl
R$*		$: $1 $| $>LookUpDomain <$&{server_name}> <?> <> <! TLS_SRV_TAG>
R$* $| <?>$*	$: $1 $| $>LookUpAddress <$&{server_addr}> <?> <> <! TLS_SRV_TAG>
dnl do a default lookup: just TLS_SRV_TAG
R$* $| <?>$*	$: $1 $| <$(access TLS_SRV_TAG`'_TAG_DELIM_ $: ? $)>
R$*		$@ $>"tls_connection" $1', `dnl
R$*		$@ $>"tls_connection" $1')

Stls_connection
ifdef(`_ACCESS_TABLE_', `dnl
dnl common ruleset for tls_{client|server}
dnl input: $&{verify} $| <ResultOfLookup> [<>]
dnl remove optional <>
R$* $| <$*>$*			$: $1 $| <$2>
dnl permanent or temporary error?
R$* $| <PERM + $={tls} $*>	$: $1 $| <503:5.7.0> <$2 $3>
R$* $| <TEMP + $={tls} $*>	$: $1 $| <403:4.7.0> <$2 $3>
dnl default case depends on TLS_PERM_ERR
R$* $| <$={tls} $*>		$: $1 $| <ifdef(`TLS_PERM_ERR', `503:5.7.0', `403:4.7.0')> <$2 $3>
dnl deal with TLS handshake failures: abort
RSOFTWARE $| <$-:$+> $* 	$#error $@ $2 $: $1 " TLS handshake failed."
dnl no <reply:dns> i.e. not requirements in the access map
dnl use default error
RSOFTWARE $| $* 		$#error $@ ifdef(`TLS_PERM_ERR', `5.7.0', `4.7.0') $: "ifdef(`TLS_PERM_ERR', `503', `403') TLS handshake failed."
R$* $| <$*> <VERIFY>		$: <$2> <VERIFY> $1
R$* $| <$*> <$={tls}:$->$*	$: <$2> <$3:$4> $1
dnl some other value in access map: accept
dnl this also allows to override the default case (if used)
R$* $| $*			$@ OK
# authentication required: give appropriate error
# other side did authenticate (via STARTTLS)
dnl workspace: <SMTP:ESC> <{VERIFY,ENCR}[:BITS]> ${verify}
dnl only verification required and it succeeded
R<$*><VERIFY> OK		$@ OK
dnl verification required + some level of encryption
R<$*><VERIFY:$-> OK		$: <$1> <REQ:$2>
dnl just some level of encryption required
R<$*><ENCR:$-> $*		$: <$1> <REQ:$2>
dnl verification required but ${verify} is not set
R<$-:$+><VERIFY $*>		$#error $@ $2 $: $1 " authentication required"
R<$-:$+><VERIFY $*> FAIL	$#error $@ $2 $: $1 " authentication failed"
R<$-:$+><VERIFY $*> NO		$#error $@ $2 $: $1 " not authenticated"
R<$-:$+><VERIFY $*> NONE	$#error $@ $2 $: $1 " other side does not support STARTTLS"
dnl some other value for ${verify}
R<$-:$+><VERIFY $*> $+		$#error $@ $2 $: $1 " authentication failure " $4
dnl some level of encryption required: get the maximum level
R<$*><REQ:$->			$: <$1> <REQ:$2> $>max $&{cipher_bits} : $&{auth_ssf}
dnl compare required bits with actual bits
R<$*><REQ:$-> $-		$: <$1> <$2:$3> $(arith l $@ $3 $@ $2 $)
R<$-:$+><$-:$-> TRUE		$#error $@ $2 $: $1 " encryption too weak " $4 " less than " $3

Smax
dnl compute the max of two values separated by :
R:		$: 0
R:$-		$: $1
R$-:		$: $1
R$-:$-		$: $(arith l $@ $1 $@ $2 $) : $1 : $2
RTRUE:$-:$-	$: $2
R$-:$-:$-	$: $2',
`dnl use default error
dnl deal with TLS handshake failures: abort
RSOFTWARE	$#error $@ ifdef(`TLS_PERM_ERR', `5.7.0', `4.7.0') $: "ifdef(`TLS_PERM_ERR', `503', `403') TLS handshake."')

SRelayAuth
# authenticated?
dnl we do not allow relaying for anyone who can present a cert
dnl signed by a "trusted" CA. For example, even if we put verisigns
dnl CA in CERTPath so we can authenticate users, we do not allow
dnl them to abuse our server (they might be easier to get hold of,
dnl but anyway).
dnl so here is the trick: if the verification succeeded
dnl we look up the cert issuer in the access map
dnl (maybe after extracting a part with a regular expression)
dnl if this returns RELAY we relay without further questions
dnl if it returns SUBJECT we perform a similar check on the
dnl cert subject.
R$* $| OK		$: $1
R$* $| $*		$@ NO		not authenticated
ifdef(`_ACCESS_TABLE_', `dnl
ifdef(`_CERT_REGEX_ISSUER_', `dnl
R$*			$: $1 $| $(CERTIssuer $&{cert_issuer} $)',
`R$*			$: $1 $| $&{cert_issuer}')
R$* $| $+		$: $1 $| $(access CERTISSUER:$2 $)
dnl use $# to stop further checks (delay_check)
R$* $| RELAY		$# RELAYCERTISSUER
ifdef(`_CERT_REGEX_SUBJECT_', `dnl
R$* $| SUBJECT		$: $1 $| <@> $(CERTSubject $&{cert_subject} $)',
`R$* $| SUBJECT		$: $1 $| <@> $&{cert_subject}')
R$* $| <@> $+		$: $1 $| <@> $(access CERTSUBJECT:$2 $)
R$* $| <@> RELAY	$# RELAYCERTSUBJECT
R$* $| $*		$: $1', `dnl')

undivert(9)dnl LOCAL_RULESETS
ifdef(`_FFR_MILTER', `
#
######################################################################
######################################################################
#####
`#####			MAIL FILTER DEFINITIONS'
#####
######################################################################
######################################################################
_MAIL_FILTERS_')
#
######################################################################
######################################################################
#####
`#####			MAILER DEFINITIONS'
#####
######################################################################
######################################################################
undivert(7)dnl MAILER_DEFINITIONS

