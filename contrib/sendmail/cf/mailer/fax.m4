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
#  This assumes you already have Sam Leffler's HylaFAX software.
#
#  Tested with HylaFAX 4.0pl1
#

ifdef(`FAX_MAILER_ARGS',,
	`define(`FAX_MAILER_ARGS', faxmail -d $u@$h $f)')
ifdef(`FAX_MAILER_PATH',,
	`define(`FAX_MAILER_PATH', /usr/local/bin/faxmail)')
ifdef(`FAX_MAILER_MAX',,
	`define(`FAX_MAILER_MAX', 100000)')
POPDIVERT
####################################
###   FAX Mailer specification   ###
####################################

VERSIONID(`@(#)fax.m4	8.11 (Berkeley) 5/19/1998')

Mfax,		P=FAX_MAILER_PATH, F=DFMhu, S=14, R=24, M=FAX_MAILER_MAX, T=X-Phone/X-FAX/X-Unix,
		A=FAX_MAILER_ARGS

LOCAL_CONFIG
CPFAX
