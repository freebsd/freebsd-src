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

ifdef(`PROCMAIL_MAILER_PATH',,
	`ifdef(`PROCMAIL_PATH',
		`define(`PROCMAIL_MAILER_PATH', PROCMAIL_PATH)',
		`define(`PROCMAIL_MAILER_PATH', /usr/local/bin/procmail)')')
ifdef(`PROCMAIL_MAILER_FLAGS',,
	`define(`PROCMAIL_MAILER_FLAGS', `SPhnu9')')
ifdef(`PROCMAIL_MAILER_ARGS',,
	`define(`PROCMAIL_MAILER_ARGS', `procmail -Y -m $h $f $u')')

POPDIVERT

######################*****##############
###   PROCMAIL Mailer specification   ###
##################*****##################

VERSIONID(`@(#)procmail.m4	8.11 (Berkeley) 5/19/98')

Mprocmail,	P=PROCMAIL_MAILER_PATH, F=CONCAT(`DFM', PROCMAIL_MAILER_FLAGS), S=11/31, R=21/31, T=DNS/RFC822/X-Unix,
		ifdef(`PROCMAIL_MAILER_MAX', `M=PROCMAIL_MAILER_MAX, ')A=PROCMAIL_MAILER_ARGS
