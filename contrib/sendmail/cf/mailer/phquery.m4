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
#  Contributed by Kimmo Suominen <kim@tac.nyc.ny.us>.
#

ifdef(`PH_MAILER_PATH',, `define(`PH_MAILER_PATH', /usr/local/etc/phquery)')
ifdef(`PH_MAILER_FLAGS',, `define(`PH_MAILER_FLAGS', `ehmu')')
ifdef(`PH_MAILER_ARGS',, `define(`PH_MAILER_ARGS', `phquery -- $u')')

POPDIVERT

####################################
###   PH Mailer specification   ###
####################################

VERSIONID(`@(#)phquery.m4	8.6 (Berkeley) 5/19/1998')

Mph,		P=PH_MAILER_PATH, F=CONCAT(`nrDFM', PH_MAILER_FLAGS), S=10, R=20/40,
		A=PH_MAILER_ARGS
