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

VERSIONID(`@(#)procmail.m4	8.6 (Berkeley) 4/30/97')

Mprocmail,	P=PROCMAIL_MAILER_PATH, F=CONCAT(`DFM', PROCMAIL_MAILER_FLAGS), S=11/31, R=21/31, T=DNS/RFC822/X-Unix,
		ifdef(`PROCMAIL_MAILER_MAX', `M=PROCMAIL_MAILER_MAX, ')A=PROCMAIL_MAILER_ARGS
