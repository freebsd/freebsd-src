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
#  Contributed by Kimmo Suominen <kim@tac.nyc.ny.us>.
#

ifdef(`_MAILER_local_', `',
	`errprint(`*** MAILER(`local') must appear before MAILER(`phquery')')')dnl

ifdef(`PH_MAILER_PATH',, `define(`PH_MAILER_PATH', /usr/local/etc/phquery)')
_DEFIFNOT(`PH_MAILER_FLAGS', `ehmu')
ifdef(`PH_MAILER_ARGS',, `define(`PH_MAILER_ARGS', `phquery -- $u')')

POPDIVERT

####################################
###   PH Mailer specification   ###
####################################

VERSIONID(`$Id: phquery.m4,v 8.15 1999/10/18 04:57:54 gshapiro Exp $')

Mph,		P=PH_MAILER_PATH, F=_MODMF_(CONCAT(`nrDFM', PH_MAILER_FLAGS), `PH'), S=EnvFromL, R=EnvToL/HdrToL,
		T=DNS/RFC822/X-Unix,
		A=PH_MAILER_ARGS
