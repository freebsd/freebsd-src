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

ifdef(`UUCP_MAILER_PATH',, `define(`UUCP_MAILER_PATH', /usr/bin/uux)')
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -a$g -gC $h!rmail ($u)')')
ifdef(`UUCP_MAILER_FLAGS',, `define(`UUCP_MAILER_FLAGS', `')')
ifdef(`UUCP_MAILER_MAX',,
	`define(`UUCP_MAILER_MAX',
		`ifdef(`UUCP_MAX_SIZE', `UUCP_MAX_SIZE', 100000)')')
POPDIVERT
#####################################
###   UUCP Mailer specification   ###
#####################################

VERSIONID(`@(#)uucp.m4	8.25 (Berkeley) 3/16/97')

#
#  There are innumerable variations on the UUCP mailer.  It really
#  is rather absurd.
#

# old UUCP mailer (two names)
Muucp,		P=UUCP_MAILER_PATH, F=CONCAT(DFMhuUd, UUCP_MAILER_FLAGS), S=12, R=22/42, M=UUCP_MAILER_MAX,
		_OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS
Muucp-old,	P=UUCP_MAILER_PATH, F=CONCAT(DFMhuUd, UUCP_MAILER_FLAGS), S=12, R=22/42, M=UUCP_MAILER_MAX,
		_OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS

# smart UUCP mailer (handles multiple addresses) (two names)
Msuucp,		P=UUCP_MAILER_PATH, F=CONCAT(mDFMhuUd, UUCP_MAILER_FLAGS), S=12, R=22/42, M=UUCP_MAILER_MAX,
		_OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS
Muucp-new,	P=UUCP_MAILER_PATH, F=CONCAT(mDFMhuUd, UUCP_MAILER_FLAGS), S=12, R=22/42, M=UUCP_MAILER_MAX,
		_OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS

ifdef(`_MAILER_smtp_',
`# domain-ized UUCP mailer
Muucp-dom,	P=UUCP_MAILER_PATH, F=CONCAT(mDFMhud, UUCP_MAILER_FLAGS), S=52/31, R=ifdef(`_ALL_MASQUERADE_', `21/31', `21'), M=UUCP_MAILER_MAX,
		_OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS

# domain-ized UUCP mailer with UUCP-style sender envelope
Muucp-uudom,	P=UUCP_MAILER_PATH, F=CONCAT(mDFMhud, UUCP_MAILER_FLAGS), S=72/31, R=ifdef(`_ALL_MASQUERADE_', `21/31', `21'), M=UUCP_MAILER_MAX,
		_OPTINS(`UUCP_MAILER_CHARSET', `C=', `, ')T=X-UUCP/X-UUCP/X-Unix,
		A=UUCP_MAILER_ARGS')


#
#  envelope and header sender rewriting
#
S12

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
S22

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
S42

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
S52

# handle error address as a special case
R<@>				$n			errors to mailer-daemon

# pass everything to standard SMTP mailer rewriting
R$*				$@ $>11 $1

#
#  envelope sender rewriting for uucp-uudom mailer
#
S72

# handle error address as a special case
R<@>				$n			errors to mailer-daemon

# do standard SMTP mailer rewriting
R$*				$: $>11 $1

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
