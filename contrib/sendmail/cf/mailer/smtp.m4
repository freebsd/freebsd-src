PUSHDIVERT(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
ifdef(`SMTP_MAILER_FLAGS',, `define(`SMTP_MAILER_FLAGS', `')')
ifdef(`SMTP_MAILER_ARGS',, `define(`SMTP_MAILER_ARGS', `IPC $h')')
ifdef(`ESMTP_MAILER_ARGS',, `define(`ESMTP_MAILER_ARGS', `IPC $h')')
ifdef(`SMTP8_MAILER_ARGS',, `define(`SMTP8_MAILER_ARGS', `IPC $h')')
ifdef(`RELAY_MAILER_ARGS',, `define(`RELAY_MAILER_ARGS', `IPC $h')')
ifdef(`_MAILER_uucp_',
	`errprint(`*** MAILER(smtp) must appear before MAILER(uucp)')')dnl
POPDIVERT
#####################################
###   SMTP Mailer specification   ###
#####################################

VERSIONID(`@(#)smtp.m4	8.38 (Berkeley) 5/19/1998')

Msmtp,		P=[IPC], F=CONCAT(mDFMuX, SMTP_MAILER_FLAGS), S=11/31, R=ifdef(`_ALL_MASQUERADE_', `21/31', `21'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=SMTP_MAILER_ARGS
Mesmtp,		P=[IPC], F=CONCAT(mDFMuXa, SMTP_MAILER_FLAGS), S=11/31, R=ifdef(`_ALL_MASQUERADE_', `21/31', `21'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=ESMTP_MAILER_ARGS
Msmtp8,		P=[IPC], F=CONCAT(mDFMuX8, SMTP_MAILER_FLAGS), S=11/31, R=ifdef(`_ALL_MASQUERADE_', `21/31', `21'), E=\r\n, L=990,
		_OPTINS(`SMTP_MAILER_MAX', `M=', `, ')_OPTINS(`SMTP_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=SMTP8_MAILER_ARGS
Mrelay,		P=[IPC], F=CONCAT(mDFMuXa8, SMTP_MAILER_FLAGS), S=11/31, R=ifdef(`_ALL_MASQUERADE_', `61/71', `61'), E=\r\n, L=2040,
		_OPTINS(`RELAY_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/SMTP,
		A=RELAY_MAILER_ARGS

#
#  envelope sender rewriting
#
S11
R$+			$: $>51 $1			sender/recipient common
R$* :; <@>		$@				list:; special case
R$*			$: $>61 $1			qualify unqual'ed names
R$+			$: $>94 $1			do masquerading


#
#  envelope recipient rewriting --
#  also header recipient if not masquerading recipients
#
S21
R$+			$: $>51 $1			sender/recipient common
R$+			$: $>61 $1			qualify unqual'ed names


#
#  header sender and masquerading header recipient rewriting
#
S31
R$+			$: $>51 $1			sender/recipient common
R:; <@>			$@				list:; special case

# do special header rewriting
R$* <@> $*		$@ $1 <@> $2			pass null host through
R< @ $* > $*		$@ < @ $1 > $2			pass route-addr through
R$*			$: $>61 $1			qualify unqual'ed names
R$+			$: $>93 $1			do masquerading


#
#  convert pseudo-domain addresses to real domain addresses
#
S51

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
#  common sender and masquerading recipient rewriting
#
S61

R$* < @ $* > $*		$@ $1 < @ $2 > $3		already fully qualified
R$+			$@ $1 < @ *LOCAL* >		add local qualification


#
#  relay mailer header masquerading recipient rewriting
#
S71

R$+			$: $>61 $1
R$+			$: $>93 $1
