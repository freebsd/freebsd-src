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
# This file is the configuration for hub.freebsd.org, the project's mail
# server.  It needs to handle some ugly mail volumes.
#

divert(0)dnl
VERSIONID(`$Id: hub.mc,v 1.1 1997/08/14 14:42:06 peter Exp $')

OSTYPE(bsd4.4)dnl
DOMAIN(generic)dnl
MAILER(local)dnl
MAILER(smtp)dnl
MASQUERADE_AS(FreeBSD.ORG)dnl
FEATURE(mailertable, `hash -o /etc/mailertable')dnl
FEATURE(masquerade_envelope)dnl
EXPOSED_USER(root)dnl
EXPOSED_USER(mailman)dnl
define(`UUCP_RELAY', uunet.uu.net)dnl
define(`BITNET_RELAY', mailhost.Berkeley.EDU)dnl
define(`CSNET_RELAY', mailhost.Berkeley.EDU)dnl
define(`confCW_FILE', `-o /etc/sendmail.cw')dnl
define(`confCHECKPOINT_INTERVAL', `4')dnl
define(`confAUTO_REBUILD', `True')dnl
define(`confMIN_FREE_BLOCKS', `1024')dnl
define(`confSMTP_MAILER', `smtp8')dnl
define(`confME_TOO', `True')dnl
define(`confMCI_CACHE_TIMEOUT', `10m')dnl
define(`confTO_QUEUEWARN', `1d')dnl
define(`confTO_QUEUEWARN_NORMAL', `1d')dnl
define(`confTO_RCPT', `10m')dnl
define(`confTO_DATABLOCK', `10m')dnl
define(`confTO_DATAFINAL', `10m')dnl
define(`confTO_COMMAND', `10m')dnl
define(`confTO_HOSTSTATUS', `30m')dnl
define(`confMIN_QUEUE_AGE', `30m')dnl
define(`confNO_RCPT_ACTION', `add-to-undisclosed')dnl
define(`confTRUSTED_USERS', `majordom')dnl
define(`confRECEIVED_HEADER', `$?sfrom $s $.$?_($?s$|from $.$_)$.
          by $j ($v/$Z)$?r with $r$. id $i$?u$|;$.
          $?ufor $u; $.$b')dnl
define(`confHOST_STATUS_DIRECTORY', `.hoststat')dnl
define(`confMAX_DAEMON_CHILDREN', `8')dnl
define(`confCONNECTION_THROTTLE_RATE', `1')dnl

LOCAL_CONFIG
Cw localhost freefall.freebsd.org
