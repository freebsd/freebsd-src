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
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `rmn9')')
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /bin/mail)')
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail -d $u')')
ifdef(`LOCAL_SHELL_FLAGS',, `define(`LOCAL_SHELL_FLAGS', `eu9')')
ifdef(`LOCAL_SHELL_PATH',, `define(`LOCAL_SHELL_PATH', /bin/sh)')
ifdef(`LOCAL_SHELL_ARGS',, `define(`LOCAL_SHELL_ARGS', `sh -c $u')')
ifdef(`LOCAL_SHELL_DIR',, `define(`LOCAL_SHELL_DIR', `$z:/')')
POPDIVERT

##################################################
###   Local and Program Mailer specification   ###
##################################################

VERSIONID(`@(#)local.m4	8.30 (Berkeley) 6/30/1998')

Mlocal,		P=LOCAL_MAILER_PATH, F=CONCAT(`lsDFMAw5:/|@q', LOCAL_MAILER_FLAGS), S=10/30, R=20/40,
		_OPTINS(`LOCAL_MAILER_MAX', `M=', `, ')_OPTINS(`LOCAL_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/X-Unix,
		A=LOCAL_MAILER_ARGS
Mprog,		P=LOCAL_SHELL_PATH, F=CONCAT(`lsDFMoq', LOCAL_SHELL_FLAGS), S=10/30, R=20/40, D=LOCAL_SHELL_DIR,
		_OPTINS(`LOCAL_MAILER_MAX', `M=', `, ')T=X-Unix,
		A=LOCAL_SHELL_ARGS

#
#  Envelope sender rewriting
#
S10
R<@>			$n			errors to mailer-daemon
R@ <@ $*>		$n			temporarily bypass Sun bogosity
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
R@ <@ $*>		$n			temporarily bypass Sun bogosity
R$+			$: $>50 $1		add local domain if needed
R$*			$: $>93 $1		do masquerading

#
#  Header recipient rewriting
#
S40
R$+			$: $>50 $1		add local domain if needed
ifdef(`_ALL_MASQUERADE_', `dnl
R$*			$: $>93 $1		do all-masquerading', `dnl')

#
#  Common code to add local domain name (only if always-add-domain)
#
S50
ifdef(`_ALWAYS_ADD_DOMAIN_', `dnl
R$* < @ $* > $* 	$@ $1 < @ $2 > $3		already fully qualified
R$+			$@ $1 < @ *LOCAL* >		add local qualification',
`dnl')
