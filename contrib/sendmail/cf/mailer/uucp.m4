PUSHDIVERT(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
ifdef(`_MAILER_smtp_', `',
	`errprint(`*** MAILER(`smtp') must appear before MAILER(`uucp')')')dnl

ifdef(`UUCP_MAILER_PATH',, `define(`UUCP_MAILER_PATH', /usr/bin/uux)')
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gC $h!rmail ($u)')')
_DEFIFNOT(`UUCP_MAILER_FLAGS', `')
ifdef(`UUCP_MAILER_MAX',,
	`define(`UUCP_MAILER_MAX',
		`ifdef(`UUCP_MAX_SIZE', `UUCP_MAX_SIZE', 100000)')')
POPDIVERT
#####################################
###   UUCP Mailer specification   ###
#####################################

VERSIONID(`$Id: uucp.m4,v 8.38 1999/10/18 04:57:55 gshapiro Exp $')

#
#  envelope and header sender rewriting
#
SFromU=12

# handle error address as a special case
R<@>				$n			errors to mailer-daemon

# list:; syntax should disappear
R:; <@>				$@

R$* < @ $* . > $*		$1 < @ $2 > $3		strip trailing dots
R$* < @ $=w >			$1			strip local name
R<@ $- . UUCP > : $+		$1 ! $2			convert to UUCP format
R<@ $+ > : $+			$1 ! $2			convert to UUCP format
R$* < @ $- . UUCP >		$2 ! $1			convert to UUCP format
R$* < @ $+ >			$2 ! $1			convert to UUCP format
R$&h ! $+ ! $+			$@ $1 ! $2		$h!...!user => ...!user
R$&h ! $+			$@ $&h ! $1		$h!user => $h!user
R$+				$: $U ! $1		prepend our name
R! $+				$: $k ! $1		in case $U undefined

#
#  envelope recipient rewriting
#
SEnvToU=22

# list:; should disappear
R:; <@>				$@

R$* < @ $* . > $*		$1 < @ $2 > $3		strip trailing dots
R$* < @ $=w >			$1			strip local name
R<@ $- . UUCP > : $+		$1 ! $2			convert to UUCP format
R<@ $+ > : $+			$1 ! $2			convert to UUCP format
R$* < @ $- . UUCP >		$2 ! $1			convert to UUCP format
R$* < @ $+ >			$2 ! $1			convert to UUCP format

#
#  header recipient rewriting
#
SHdrToU=42

# list:; syntax should disappear
R:; <@>				$@

R$* < @ $* . > $*		$1 < @ $2 > $3		strip trailing dots
R$* < @ $=w >			$1			strip local name
R<@ $- . UUCP > : $+		$1 ! $2			convert to UUCP format
R<@ $+ > : $+			$1 ! $2			convert to UUCP format
R$* < @ $- . UUCP >		$2 ! $1			convert to UUCP format
R$* < @ $+ >			$2 ! $1			convert to UUCP format
R$&h ! $+ ! $+			$@ $1 ! $2		$h!...!user => ...!user
R$&h ! $+			$@ $&h ! $1		$h!user => $h!user
R$+				$: $U ! $1		prepend our name
R! $+				$: $k ! $1		in case $U undefined


ifdef(`_MAILER_smtp_',
`#
#  envelope sender rewriting for uucp-dom mailer
#
SEnvFromUD=52

# handle error address as a special case
R<@>				$n			errors to mailer-daemon

# pass everything to standard SMTP mailer rewriting
R$*				$@ $>EnvFromSMTP $1

#
#  envelope sender rewriting for uucp-uudom mailer
#
SEnvFromUUD=72

# handle error address as a special case
R<@>				$n			errors to mailer-daemon

# do standard SMTP mailer rewriting
R$*				$: $>EnvFromSMTP $1

R$* < @ $* . > $*		$1 < @ $2 > $3		strip trailing dots
R<@ $- . UUCP > : $+		$@ $1 ! $2		convert to UUCP format
R<@ $+ > : $+			$@ $1 ! $2		convert to UUCP format
R$* < @ $- . UUCP >		$@ $2 ! $1		convert to UUCP format
R$* < @ $+ >			$@ $2 ! $1		convert to UUCP format')


PUSHDIVERT(4)
# resolve locally connected UUCP links
R$* < @ $=Z . UUCP. > $*	$#uucp-uudom $@ $2 $: $1 < @ $2 .UUCP. > $3
R$* < @ $=Y . UUCP. > $*	$#uucp-new $@ $2 $: $1 < @ $2 .UUCP. > $3
R$* < @ $=U . UUCP. > $*	$#uucp-old $@ $2 $: $1 < @ $2 .UUCP. > $3
POPDIVERT

#
#  There are innumerable variations on the UUCP mailer.  It really
#  is rather absurd.
#

# old UUCP mailer (two names)
Muucp,		P=UUCP_MAILER_PATH, F=_MODMF_(CONCAT(`DFMhuUd', UUCP_MAILER_FLAGS), `UUCP'), S=FromU, R=EnvToU/HdrToU,
		M=UUCP_MAILER_MAX, _OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS
Muucp-old,	P=UUCP_MAILER_PATH, F=_MODMF_(CONCAT(`DFMhuUd', UUCP_MAILER_FLAGS), `UUCP'), S=FromU, R=EnvToU/HdrToU,
		M=UUCP_MAILER_MAX, _OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS

# smart UUCP mailer (handles multiple addresses) (two names)
Msuucp,		P=UUCP_MAILER_PATH, F=_MODMF_(CONCAT(`mDFMhuUd', UUCP_MAILER_FLAGS), `UUCP'), S=FromU, R=EnvToU/HdrToU,
		 M=UUCP_MAILER_MAX, _OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS
Muucp-new,	P=UUCP_MAILER_PATH, F=_MODMF_(CONCAT(`mDFMhuUd', UUCP_MAILER_FLAGS), `UUCP'), S=FromU, R=EnvToU/HdrToU,
		M=UUCP_MAILER_MAX, _OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS

ifdef(`_MAILER_smtp_',
`# domain-ized UUCP mailer
Muucp-dom,	P=UUCP_MAILER_PATH, F=_MODMF_(CONCAT(`mDFMhud', UUCP_MAILER_FLAGS), `UUCP'), S=EnvFromUD/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `EnvToSMTP/HdrFromSMTP', `EnvToSMTP'),
		M=UUCP_MAILER_MAX, _OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS

# domain-ized UUCP mailer with UUCP-style sender envelope
Muucp-uudom,	P=UUCP_MAILER_PATH, F=_MODMF_(CONCAT(`mDFMhud', UUCP_MAILER_FLAGS), `UUCP'), S=EnvFromUUD/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `EnvToSMTP/HdrFromSMTP', `EnvToSMTP'),
		M=UUCP_MAILER_MAX, _OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS')


