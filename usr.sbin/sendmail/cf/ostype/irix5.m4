divert(-1)
#
# Copyright (c) 1995 Eric P. Allman
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
#  Contributed by Kari E. Hurtta <Kari.Hurtta@dionysos.fmi.fi>
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

#
# Notes:
# - SGI's /etc/sendmail.cf defines also 'u' for local mailer flags -- you
#   perhaps don't want it.
# - Perhaps is should also add define(`LOCAL_MAILER_CHARSET', iso-8859-1)
#   put some Asian sites may prefer otherwise -- or perhaps not.
# - SGI's /etc/sendmail.cf seems use: A=mail -s -d $u
#   It seems work without that -s however.
# - SGI's /etc/sendmail.cf set's default uid and gid to 998 (guest)
# - In SGI seems that TZ variable is needed that correct time is marked to
#   syslog
# - helpfile is in /etc/sendmail.hf in SGI's /etc/sendmail.cf
#

divert(0)
VERSIONID(`@(#)irix5.m4	8.2 (Berkeley) 11/13/95')
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', Ehmu)')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail -s -d $u')')dnl
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /var/spool/mqueue)')dnl
ifdef(`ALIAS_FILE',, `define(`ALIAS_FILE', /etc/aliases)')dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', /var/sendmail.st)')dnl
ifdef(`HELP_FILE',, `define(`HELP_FILE', /etc/sendmail.hf)')dnl
define(`confDEF_USER_ID', `998:998')dnl
define(`confTIME_ZONE', USE_TZ)dnl
