PUSHDIVERT(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
# Not exciting enough to bother with copyrights and most of the
# rulesets are based from those provided by DEC.
# Barb Dijker, Labyrinth Computer Services, barb@labyrinth.com
#
# This mailer is only useful if you have DECNET and the
# mail11 program - gatekeeper.dec.com:/pub/DEC/gwtools.
#
# For local delivery of DECNET style addresses to the local
# DECNET node, you will need feature(use_cw_file) and put
# your DECNET nodename in in the cw file.
#
ifdef(`MAIL11_MAILER_PATH',, `define(`MAIL11_MAILER_PATH', /usr/etc/mail11)')
_DEFIFNOT(`MAIL11_MAILER_FLAGS', `nsFx')
ifdef(`MAIL11_MAILER_ARGS',, `define(`MAIL11_MAILER_ARGS', mail11 $g $x $h $u)')
define(`_USE_DECNET_SYNTAX_')
define(`_LOCAL_', ifdef(`confLOCAL_MAILER', confLOCAL_MAILER, `local'))

POPDIVERT

PUSHDIVERT(3)
# DECNET delivery
R$* < @ $=w .DECNET. >		$#_LOCAL_ $: $1			local DECnet
R$+ < @ $+ .DECNET. >		$#mail11 $@ $2 $: $1		DECnet user
POPDIVERT

PUSHDIVERT(6)
CPDECNET
POPDIVERT

###########################################
###   UTK-MAIL11 Mailer specification   ###
###########################################

VERSIONID(`$Id: mail11.m4,v 8.19 1999/10/18 04:57:54 gshapiro Exp $')

SMail11From=15
R$+			$: $>25 $1		preprocess
R$w :: $+		$@ $w :: $1		ready to go

SMail11To=25
R$+ < @ $- .UUCP >	$: $2 ! $1		back to old style
R$+ < @ $- .DECNET >	$: $2 :: $1		convert to DECnet style
R$+ < @ $- .LOCAL >	$: $2 :: $1		convert to DECnet style
R$+ < @ $=w. >		$: $2 :: $1		convert to DECnet style
R$=w :: $+		$2			strip local names
R$+ :: $+		$@ $1 :: $2		already qualified

Mmail11, P=MAIL11_MAILER_PATH, F=_MODMF_(MAIL11_MAILER_FLAGS, `MAIL11'), S=Mail11From, R=Mail11To,
	T=DNS/X-DECnet/X-Unix,
	A=MAIL11_MAILER_ARGS
