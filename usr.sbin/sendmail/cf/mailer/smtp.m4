PUSHDIVERT(-1)
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

VERSIONID(`@(#)smtp.m4	8.32 (Berkeley) 11/20/95')

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
R< $+ ! > $+		$: $1 ! $2 < @ *LOCAL* >')


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
