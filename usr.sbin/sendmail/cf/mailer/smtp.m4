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
ifdef(`SMTP_MAILER_FLAGS',,
	`define(`SMTP_MAILER_FLAGS',
		`ifdef(`_OLD_SENDMAIL_', `L', `')')')
POPDIVERT
#####################################
###   SMTP Mailer specification   ###
#####################################

VERSIONID(`@(#)smtp.m4	8.3 (Berkeley) 7/11/93')

Msmtp,		P=[IPC], F=CONCAT(mDFMueXC, SMTP_MAILER_FLAGS), S=11, R=ifdef(`_ALL_MASQUERADE_', `11', `21'), E=\r\n,
		ifdef(`_OLD_SENDMAIL_',, `L=990, ')A=IPC $h
Mesmtp,		P=[IPC], F=CONCAT(mDFMueXCa, SMTP_MAILER_FLAGS), S=11, R=ifdef(`_ALL_MASQUERADE_', `11', `21'), E=\r\n,
		ifdef(`_OLD_SENDMAIL_',, `L=990, ')A=IPC $h
Mrelay,		P=[IPC], F=CONCAT(mDFMueXCa, SMTP_MAILER_FLAGS), S=11, R=19, E=\r\n,
		ifdef(`_OLD_SENDMAIL_',, `L=2040, ')A=IPC $h

S11

# do sender/recipient common rewriting
R$+			$: $>19 $1

# if already @ qualified, we are done
R$* < @ $* > $*		$@ $1 < @ $2 > $3		already qualified

# do not qualify list:; syntax
R$* :; <@>		$@ $1 :;

# unqualified names (e.g., "eric") "come from" $M
R$=E			$@ $1 < @ $j>			show exposed names
R$+			$: $1 < @ $M >			user w/o host
R$+ <@>			$: $1 < @ $j >			in case $M undefined

ifdef(`_ALL_MASQUERADE_', `dnl',
`S21

# do sender/recipient common rewriting
R$+			$: $>19 $1

# if already @ qualified, we are done
R$* < @ $* > $*		$@ $1 < @ $2 > $3		already qualified

# do not qualify list:; syntax
R$* :; <@>		$@ $1 :;

# unqualified names (e.g., "eric") are qualified by local host
R$+			$: $1 < @ $j >')

S19

# pass <route-addr>s through
R< @ $+ > $*		$@ < @ $1 > $2			resolve <route-addr>

# output fake domains as user%fake@relay
ifdef(`BITNET_RELAY',
`R$+ <@ $+ . BITNET >	$: $1 % $2 .BITNET < @ $B >	user@host.BITNET',
	`dnl')
ifdef(`CSNET_RELAY',
`R$+ <@ $+ . CSNET >	$: $1 % $2 .CSNET < @ $C >	user@host.CSNET',
	`dnl')
ifdef(`_NO_UUCP_', `dnl',
`R$+ <@ $+ . UUCP >	$: $2 ! $1 < @ $j >		user@host.UUCP')
