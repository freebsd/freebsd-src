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
#  This is a generic configuration file for FreeBSD 4.X and later systems.
#  If you want to customize it, copy it to a name appropriate for your
#  environment and do the modifications there.
#
#  The best documentation for this .mc file is:
#  /usr/share/sendmail/cf/README or
#  /usr/src/contrib/sendmail/cf/README
#

divert(0)
VERSIONID(`$FreeBSD$')
OSTYPE(freebsd4)
DOMAIN(generic)

FEATURE(access_db, `hash -o -T<TMPF> /etc/mail/access')
FEATURE(blacklist_recipients)
FEATURE(local_lmtp)
FEATURE(mailertable, `hash -o /etc/mail/mailertable')
FEATURE(virtusertable, `hash -o /etc/mail/virtusertable')

dnl Uncomment to allow relaying based on your MX records.
dnl NOTE: This can allow sites to use your server as a backup MX without
dnl       your permission.
dnl FEATURE(relay_based_on_MX)

dnl DNS based black hole lists
dnl --------------------------------
dnl DNS based black hole lists come and go on a regular basis
dnl so this file will not serve as a database of the available servers.
dnl For that, visit http://dmoz.org/Computers/Internet/Abuse/Spam/Blacklists/

dnl Uncomment to activate Realtime Blackhole List
dnl information available at http://www.mail-abuse.com/
dnl NOTE: This is a subscription service as of July 31, 2001
dnl FEATURE(dnsbl)
dnl Alternatively, you can provide your own server and rejection message:
dnl FEATURE(dnsbl, `blackholes.mail-abuse.org', `"550 Mail from " $&{client_addr} " rejected, see http://mail-abuse.org/cgi-bin/lookup?" $&{client_addr}')

dnl Dialup users should uncomment and define this appropriately
dnl define(`SMART_HOST', `your.isp.mail.server')

dnl Uncomment the first line to change the location of the default
dnl /etc/mail/local-host-names and comment out the second line.
dnl define(`confCW_FILE', `-o /etc/mail/sendmail.cw')
define(`confCW_FILE', `-o /etc/mail/local-host-names')

dnl Uncomment both of the following lines to listen on IPv6 as well as IPv4
dnl DAEMON_OPTIONS(`Name=IPv4, Family=inet')
dnl DAEMON_OPTIONS(`Name=IPv6, Family=inet6')

define(`confBIND_OPTS', `WorkAroundBrokenAAAA')
define(`confMAX_MIME_HEADER_LENGTH', `256/128')
define(`confNO_RCPT_ACTION', `add-to-undisclosed')
define(`confPRIVACY_FLAGS', `authwarnings,noexpn,novrfy')
MAILER(local)
MAILER(smtp)
