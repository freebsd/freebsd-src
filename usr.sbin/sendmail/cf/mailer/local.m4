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
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `rmn')')
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /bin/mail)')
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail -d $u')')
ifdef(`LOCAL_SHELL_FLAGS',, `define(`LOCAL_SHELL_FLAGS', `eu')')
ifdef(`LOCAL_SHELL_PATH',, `define(`LOCAL_SHELL_PATH', /bin/sh)')
ifdef(`LOCAL_SHELL_ARGS',, `define(`LOCAL_SHELL_ARGS', `sh -c $u')')
ifdef(`LOCAL_SHELL_DIR',, `define(`LOCAL_SHELL_DIR', `$z:/')')
POPDIVERT

##################################################
###   Local and Program Mailer specification   ###
##################################################

VERSIONID(`@(#)local.m4	8.21 (Berkeley) 11/6/95')

Mlocal,		P=LOCAL_MAILER_PATH, F=CONCAT(`lsDFMAw5:/|@', LOCAL_MAILER_FLAGS), S=10/30, R=20/40,
		_OPTINS(`LOCAL_MAILER_MAX', `M=', `, ')_OPTINS(`LOCAL_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/X-Unix,
		A=LOCAL_MAILER_ARGS
Mprog,		P=LOCAL_SHELL_PATH, F=CONCAT(`lsDFMo', LOCAL_SHELL_FLAGS), S=10/30, R=20/40, D=LOCAL_SHELL_DIR,
		_OPTINS(`LOCAL_MAILER_MAX', `M=', `, ')T=X-Unix,
		A=LOCAL_SHELL_ARGS

#
#  Envelope sender rewriting
#
S10
R<@>			$n			errors to mailer-daemon
R$+			$: $>50 $1		add local domain if needed
R$*			$: $>94 $1		do masquerading

#
#  Envelope recipient rewriting
#
S20
R$+ < @ $* >		$: $1			strip host part

#
#  Header sender rewriting
#
S30
R<@>			$n			errors to mailer-daemon
R$+			$: $>50 $1		add local domain if needed
R$*			$: $>93 $1		do masquerading

#
#  Header recipient rewriting
#
S40
R$+			$: $>50 $1		add local domain if needed
ifdef(`_ALL_MASQUERADE_', `', `#')dnl
R$*			$: $>93 $1		do all-masquerading

#
#  Common code to add local domain name (only if always-add-domain)
#
S50
ifdef(`_ALWAYS_ADD_DOMAIN_', `', `#')dnl
R$* < @ $* > $* 	$@ $1 < @ $2 > $3		already fully qualified
ifdef(`_ALWAYS_ADD_DOMAIN_', `', `#')dnl
R$+			$@ $1 < @ *LOCAL* >		add local qualification
