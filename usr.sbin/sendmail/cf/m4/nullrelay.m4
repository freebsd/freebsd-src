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

VERSIONID(`@(#)nullrelay.m4	8.5 (Berkeley) 2/1/94')

#
#  This configuration applies only to relay-only hosts.  They send
#  all mail to a hub without consideration of the address syntax
#  or semantics, except for adding the hub qualification to the
#  addresses.
#
#	This is based on a prototype done by Bryan Costales of ICSI.
#

# hub host (to which all mail is sent)
DH`'ifdef(`MAIL_HUB', MAIL_HUB,
	`errprint(`MAIL_HUB not defined for nullclient feature')')

# name from which everyone will appear to come
DM`'ifdef(`MASQUERADE_NAME', MASQUERADE_NAME, MAIL_HUB)

# route-addr separators
C: : ,

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
Dq<$g>
include(`../m4/version.m4')

###############
#   Options   #
###############

# strip message body to 7 bits on input?
O7`'confSEVEN_BIT_INPUT

# no aliases here

# substitution for space (blank) characters
OB`'confBLANK_SUB

# default delivery mode
Od`'confDELIVERY_MODE

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
HReceived: $?sfrom $s $.$?_($_) $.by $j ($v/$Z)$?r with $r$. id $i$?u for $u$.; $b
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

###########################################
###  Rulset 3 -- Name Canonicalization  ###
###########################################
S3

# handle null input and list syntax (translate to <@> special case)
R$@			$@ <@>
R$*:;$*			$@ $1 :; <@>

# basic textual canonicalization -- note RFC733 heuristic here
R$*<$*>$*<$*>$*		$2$3<$4>$5			strip multiple <> <>
R$*<$*<$+>$*>$*		<$3>$5				2-level <> nesting
R$*<>$*			$@ <@>				MAIL FROM:<> case
R$*<$+>$*		$2				basic RFC821/822 parsing

ifdef(`_NO_CANONIFY_', `dnl',
`# eliminate local host if present
R@ $=w $=: $+		$@ @ $M $2 $3			@thishost ...
R@ $+			$@ @ $1				@somewhere ...

R$+ @ $=w		$@ $1 @ $M			...@thishost
R$+ @ $+		$@ $1 @ $2			...@somewhere

R$=w ! $+		$@ $2 @ $M			thishost!...
R$+ ! $+		$@ $1 ! $2 @ $M			somewhere ! ...

R$+ % $=w		$@ $1 @ $M			...%thishost
R$+ % $+		$@ $1 @ $2			...%somewhere

R$+			$@ $1 @ $M			unadorned user')


######################################
###   Ruleset 0 -- Parse Address   ###
######################################

S0

R$*:;<@>		$#error $@ USAGE $: "list:; syntax illegal for recipient addresses"

# pass everything else to a relay host
R$*			$#_RELAY_ $@ $H $: $1

#
######################################################################
######################################################################
#####
`#####			MAILER DEFINITIONS'
#####
######################################################################
######################################################################
undivert(7)dnl
