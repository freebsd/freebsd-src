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

ifdef(`POP_MAILER_PATH',, `define(`POP_MAILER_PATH', /usr/lib/mh/spop)')
ifdef(`POP_MAILER_FLAGS',, `define(`POP_MAILER_FLAGS', `Penu')')
ifdef(`POP_MAILER_ARGS',, `define(`POP_MAILER_ARGS', `pop $u')')

POPDIVERT

####################################
###   POP Mailer specification   ###
####################################

VERSIONID(`@(#)pop.m4	8.11 (Berkeley) 5/19/98')

Mpop,		P=POP_MAILER_PATH, F=CONCAT(`lsDFMq', POP_MAILER_FLAGS), S=10, R=20/40, T=DNS/RFC822/X-Unix,
		A=POP_MAILER_ARGS

LOCAL_CONFIG
# POP mailer is a pseudo-domain
CPPOP
