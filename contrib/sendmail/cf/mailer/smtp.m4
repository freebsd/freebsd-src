PUSHDIVERT(-1)
#
# Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
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
_DEFIFNOT(`_DEF_SMTP_MAILER_FLAGS', `mDFMuX')
_DEFIFNOT(`SMTP_MAILER_FLAGS',`')
_DEFIFNOT(`RELAY_MAILER_FLAGS', `SMTP_MAILER_FLAGS')
ifdef(`SMTP_MAILER_ARGS',, `define(`SMTP_MAILER_ARGS', `TCP $h')')
ifdef(`ESMTP_MAILER_ARGS',, `define(`ESMTP_MAILER_ARGS', `TCP $h')')
ifdef(`SMTP8_MAILER_ARGS',, `define(`SMTP8_MAILER_ARGS', `TCP $h')')
ifdef(`DSMTP_MAILER_ARGS',, `define(`DSMTP_MAILER_ARGS', `TCP $h')')
ifdef(`RELAY_MAILER_ARGS',, `define(`RELAY_MAILER_ARGS', `TCP $h')')
POPDIVERT
#####################################
###   SMTP Mailer specification   ###
#####################################

VERSIONID(`$Id: smtp.m4,v 8.56.2.1.2.3 2000/09/25 13:53:27 ca Exp $')

#
#  common sender and masquerading recipient rewriting
#
SMasqSMTP=61
R$* < @ $* > $*		$@ $1 < @ $2 > $3		already fully qualified
R$+			$@ $1 < @ *LOCAL* >		add local qualification

#
#  convert pseudo-domain addresses to real domain addresses
#
SPseudoToReal=51

# pass <route-addr>s through
R< @ $+ > $*		$@ < @ $1 > $2			resolve <route-addr>

# output fake domains as user%fake@relay
ifdef(`BITNET_RELAY',
`R$+ <@ $+ .BITNET. >	$: $1 % $2 .BITNET < @ $B >	user@host.BITNET
R$+.BITNET <@ $+:$+ >	$: $1 .BITNET < @ $3 >		strip mailer: part',
	`dnl')
ifdef(`_NO_UUCP_', `dnl', `
# do UUCP heuristics; note that these are shared with UUCP mailers
R$+ < @ $+ .UUCP. >	$: < $2 ! > $1			convert to UUCP form
R$+ < @ $* > $*		$@ $1 < @ $2 > $3		not UUCP form

# leave these in .UUCP form to avoid further tampering
R< $&h ! > $- ! $+	$@ $2 < @ $1 .UUCP. >
R< $&h ! > $-.$+ ! $+	$@ $3 < @ $1.$2 >
R< $&h ! > $+		$@ $1 < @ $&h .UUCP. >
R< $+ ! > $+		$: $1 ! $2 < @ $Y >		use UUCP_RELAY
R$+ < @ $+ : $+ >	$@ $1 < @ $3 >			strip mailer: part
R$+ < @ >		$: $1 < @ *LOCAL* >		if no UUCP_RELAY')


#
#  envelope sender rewriting
#
SEnvFromSMTP=11
R$+			$: $>PseudoToReal $1		sender/recipient common
R$* :; <@>		$@				list:; special case
R$*			$: $>MasqSMTP $1		qualify unqual'ed names
R$+			$: $>MasqEnv $1			do masquerading


#
#  envelope recipient rewriting --
#  also header recipient if not masquerading recipients
#
SEnvToSMTP=21
R$+			$: $>PseudoToReal $1		sender/recipient common
R$+			$: $>MasqSMTP $1		qualify unqual'ed names
R$* < @ *LOCAL* > $*	$: $1 < @ $j . > $2

#
#  header sender and masquerading header recipient rewriting
#
SHdrFromSMTP=31
R$+			$: $>PseudoToReal $1		sender/recipient common
R:; <@>			$@				list:; special case

# do special header rewriting
R$* <@> $*		$@ $1 <@> $2			pass null host through
R< @ $* > $*		$@ < @ $1 > $2			pass route-addr through
R$*			$: $>MasqSMTP $1		qualify unqual'ed names
R$+			$: $>MasqHdr $1			do masquerading


#
#  relay mailer header masquerading recipient rewriting
#
SMasqRelay=71
R$+			$: $>MasqSMTP $1
R$+			$: $>MasqHdr $1

Msmtp,		P=[IPC], F=_MODMF_(CONCAT(_DEF_SMTP_MAILER_FLAGS, SMTP_MAILER_FLAGS), `SMTP'), S=EnvFromSMTP/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `EnvToSMTP/HdrFromSMTP', `EnvToSMTP'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_MAXMSGS', `m=', `, ')_OPTINS(`SMTP_MAILER_MAXRCPTS', `r=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=SMTP_MAILER_ARGS
Mesmtp,		P=[IPC], F=_MODMF_(CONCAT(_DEF_SMTP_MAILER_FLAGS, `a', SMTP_MAILER_FLAGS), `SMTP'), S=EnvFromSMTP/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `EnvToSMTP/HdrFromSMTP', `EnvToSMTP'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_MAXMSGS', `m=', `, ')_OPTINS(`SMTP_MAILER_MAXRCPTS', `r=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=ESMTP_MAILER_ARGS
Msmtp8,		P=[IPC], F=_MODMF_(CONCAT(_DEF_SMTP_MAILER_FLAGS, `8', SMTP_MAILER_FLAGS), `SMTP'), S=EnvFromSMTP/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `EnvToSMTP/HdrFromSMTP', `EnvToSMTP'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_MAXMSGS', `m=', `, ')_OPTINS(`SMTP_MAILER_MAXRCPTS', `r=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=SMTP8_MAILER_ARGS
Mdsmtp,		P=[IPC], F=_MODMF_(CONCAT(_DEF_SMTP_MAILER_FLAGS, `a%', SMTP_MAILER_FLAGS), `SMTP'), S=EnvFromSMTP/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `EnvToSMTP/HdrFromSMTP', `EnvToSMTP'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_MAXMSGS', `m=', `, ')_OPTINS(`SMTP_MAILER_MAXRCPTS', `r=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=DSMTP_MAILER_ARGS
Mrelay,		P=[IPC], F=_MODMF_(CONCAT(_DEF_SMTP_MAILER_FLAGS, `a8', RELAY_MAILER_FLAGS), `RELAY'), S=EnvFromSMTP/HdrFromSMTP, R=ifdef(`_ALL_MASQUERADE_', `MasqSMTP/MasqRelay', `MasqSMTP'), E=\r\n, L=2040,
		_OPTINS(`RELAY_MAILER_CHARSET', `C=', `, ')_OPTINS(`RELAY_MAILER_MAXMSGS', `m=', `, ')_OPTINS(`SMTP_MAILER_MAXRCPTS', `r=', `, ')T=DNS/RFC822/SMTP,
		A=RELAY_MAILER_ARGS
