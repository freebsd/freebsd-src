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
_DEFIFNOT(`_DEF_LOCAL_MAILER_FLAGS', `lsDFMAw5:/|@q')
_DEFIFNOT(`LOCAL_MAILER_FLAGS', `Prmn9')
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /bin/mail)')
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail -d $u')')
ifdef(`LOCAL_MAILER_DSN_DIAGNOSTIC_CODE',, `define(`LOCAL_MAILER_DSN_DIAGNOSTIC_CODE', `X-Unix')')
_DEFIFNOT(`_DEF_LOCAL_SHELL_FLAGS', `lsDFMoq')
_DEFIFNOT(`LOCAL_SHELL_FLAGS', `eu9')
ifdef(`LOCAL_SHELL_PATH',, `define(`LOCAL_SHELL_PATH', /bin/sh)')
ifdef(`LOCAL_SHELL_ARGS',, `define(`LOCAL_SHELL_ARGS', `sh -c $u')')
ifdef(`LOCAL_SHELL_DIR',, `define(`LOCAL_SHELL_DIR', `$z:/')')
POPDIVERT

##################################################
###   Local and Program Mailer specification   ###
##################################################

VERSIONID(`$Id: local.m4,v 8.50.16.1 2000/06/12 18:25:40 gshapiro Exp $')

#
#  Envelope sender rewriting
#
SEnvFromL=10
R<@>			$n			errors to mailer-daemon
R@ <@ $*>		$n			temporarily bypass Sun bogosity
R$+			$: $>AddDomain $1	add local domain if needed
R$*			$: $>MasqEnv $1		do masquerading

#
#  Envelope recipient rewriting
#
SEnvToL=20
R$+ < @ $* >		$: $1			strip host part
ifdef(`_FFR_ADDR_TYPE', `dnl
ifdef(`confUSERDB_SPEC', `dnl',
`dnl Do not forget to bump V9 to V10 before removing _FFR_ADDR_TYPE check
R$+ + $*		$: < $&{addr_type} > $1 + $2	mark with addr type
R<e s> $+ + $*		$: $1			remove +detail for sender
R< $* > $+		$: $2			else remove mark')', `dnl')

#
#  Header sender rewriting
#
SHdrFromL=30
R<@>			$n			errors to mailer-daemon
R@ <@ $*>		$n			temporarily bypass Sun bogosity
R$+			$: $>AddDomain $1	add local domain if needed
R$*			$: $>MasqHdr $1		do masquerading

#
#  Header recipient rewriting
#
SHdrToL=40
R$+			$: $>AddDomain $1	add local domain if needed
ifdef(`_ALL_MASQUERADE_',
`R$*			$: $>MasqHdr $1		do all-masquerading',
`R$* < @ *LOCAL* > $*	$: $1 < @ $j . > $2')

#
#  Common code to add local domain name (only if always-add-domain)
#
SAddDomain=50
ifdef(`_ALWAYS_ADD_DOMAIN_', `dnl
R$* < @ $* > $* 	$@ $1 < @ $2 > $3	already fully qualified
R$+			$@ $1 < @ *LOCAL* >	add local qualification',
`dnl')

Mlocal,		P=LOCAL_MAILER_PATH, F=_MODMF_(CONCAT(_DEF_LOCAL_MAILER_FLAGS, LOCAL_MAILER_FLAGS), `LOCAL'), S=EnvFromL/HdrFromL, R=EnvToL/HdrToL,_OPTINS(`LOCAL_MAILER_EOL', ` E=', `, ')
		_OPTINS(`LOCAL_MAILER_MAX', `M=', `, ')_OPTINS(`LOCAL_MAILER_MAXMSGS', `m=', `, ')_OPTINS(`LOCAL_MAILER_MAXRCPTS', `r=', `, ')_OPTINS(`LOCAL_MAILER_CHARSET', `C=', `, ')T=DNS/RFC822/LOCAL_MAILER_DSN_DIAGNOSTIC_CODE,
		A=LOCAL_MAILER_ARGS
Mprog,		P=LOCAL_SHELL_PATH, F=CONCAT(_DEF_LOCAL_SHELL_FLAGS, LOCAL_SHELL_FLAGS), S=EnvFromL/HdrFromL, R=EnvToL/HdrToL, D=LOCAL_SHELL_DIR,
		_OPTINS(`LOCAL_MAILER_MAX', `M=', `, ')T=X-Unix/X-Unix/X-Unix,
		A=LOCAL_SHELL_ARGS
