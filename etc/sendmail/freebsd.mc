divert(-1)
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

#
#  This is a generic configuration file for 4.4 BSD-based systems.
#  If you want to customize it, copy it to a name appropriate for your
#  environment and do the modifications there.
#
#  The best documentation for this .mc file is:
#  /usr/src/contrib/sendmail/cf/README
#

divert(0)dnl
VERSIONID(`$FreeBSD$')
OSTYPE(bsd4.4)dnl
DOMAIN(generic)dnl
FEATURE(relay_based_on_MX)dnl
FEATURE(mailertable, `hash -o /etc/mail/mailertable')dnl
FEATURE(access_db, `hash -o /etc/mail/access')dnl
FEATURE(blacklist_recipients)dnl
FEATURE(virtusertable, `hash -o /etc/mail/virtusertable')dnl
dnl Uncomment to activate Realtime Blackhole List (recommended!)
dnl information available at http://maps.vix.com/rbl/
dnl FEATURE(dnsbl)dnl
dnl Many sites reject email connections from dialup ip addresses
dnl by using the MAPS Dial-up User List (DUL).  http://maps.vix.com/dul/
dnl Dialup users should uncomment and define this appropriately
dnl define(`SMART_HOST', `your.isp.mail.server')dnl
FEATURE(local_lmtp)dnl
define(`LOCAL_MAILER_FLAGS', LOCAL_MAILER_FLAGS`'P)dnl
dnl Uncomment the first line to change the location of the default
dnl /etc/mail/local-host-names and comment out the second line.
dnl define(`confCW_FILE', `-o /etc/mail/sendmail.cw')dnl
define(`confCW_FILE', `-o /etc/mail/local-host-names')dnl
define(`confNO_RCPT_ACTION', `add-to-undisclosed')dnl
define(`confMAX_MIME_HEADER_LENGTH', `256/128')dnl
MAILER(local)dnl
MAILER(smtp)dnl
