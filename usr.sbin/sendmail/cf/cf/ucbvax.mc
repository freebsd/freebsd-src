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
#  This machine has been decommissioned at Berkeley, and hence should
#  not be considered to be tested.  This file is provided as an example
#  only, of how you might set up a fairly complex configuration.
#  Ucbvax was our main relay (both SMTP and UUCP) for many years.
#  At this point I recommend using `FEATURE(mailertable)' instead of
#  `SITECONFIG' for routing of UUCP within your domain.
#

VERSIONID(`@(#)ucbvax.mc	8.5 (Berkeley) 8/25/95')
OSTYPE(bsd4.3)
DOMAIN(CS.Berkeley.EDU)
MASQUERADE_AS(CS.Berkeley.EDU)
MAILER(local)
MAILER(smtp)
MAILER(uucp)
undefine(`UUCP_RELAY')dnl

LOCAL_CONFIG
DDBerkeley.EDU

# names for which we act as a local forwarding agent
CF CS
FF/etc/sendmail.cw

# local UUCP connections, and our local uucp name
SITECONFIG(uucp.ucbvax, ucbvax, U)

# remote UUCP connections, and the machine they are on
SITECONFIG(uucp.ucbarpa, ucbarpa.Berkeley.EDU, W)

SITECONFIG(uucp.cogsci, cogsci.Berkeley.EDU, X)

LOCAL_RULE_3
# map old UUCP names into Internet names
UUCPSMTP(bellcore,	bellcore.com)
UUCPSMTP(decvax,	decvax.dec.com)
UUCPSMTP(decwrl,	decwrl.dec.com)
UUCPSMTP(hplabs,	hplabs.hp.com)
UUCPSMTP(lbl-csam,	lbl-csam.arpa)
UUCPSMTP(pur-ee,	ecn.purdue.edu)
UUCPSMTP(purdue,	purdue.edu)
UUCPSMTP(research,	research.att.com)
UUCPSMTP(sdcarl,	sdcarl.ucsd.edu)
UUCPSMTP(sdcsvax,	sdcsvax.ucsd.edu)
UUCPSMTP(ssyx,		ssyx.ucsc.edu)
UUCPSMTP(sun,		sun.com)
UUCPSMTP(ucdavis,	ucdavis.ucdavis.edu)
UUCPSMTP(ucivax,	ics.uci.edu)
UUCPSMTP(ucla-cs,	cs.ucla.edu)
UUCPSMTP(ucla-se,	seas.ucla.edu)
UUCPSMTP(ucsbcsl,	ucsbcsl.ucsb.edu)
UUCPSMTP(ucscc,		c.ucsc.edu)
UUCPSMTP(ucsd,		ucsd.edu)
UUCPSMTP(ucsfcgl,	cgl.ucsf.edu)
UUCPSMTP(unmvax,	unmvax.cs.unm.edu)
UUCPSMTP(uwvax,		spool.cs.wisc.edu)

LOCAL_RULE_0

# make sure we handle the local domain as absolute
R$* <  @ $* $D > $*		$: $1 < @ $2 $D . > $3

# handle names we forward for as though they were local, so we will use UDB
R< @ $=F . $D . > : $*		$@ $>7 $2		@here:... -> ...
R< @ $D . > : $*		$@ $>7 $1		@here:... -> ...
R$* $=O $* < @ $=F . $D . >	$@ $>7 $1 $2 $3		...@here -> ...
R$* $=O $* < @ $D . >		$@ $>7 $1 $2 $3		...@here -> ...

R$* < @ $=F . $D . >		$#local $: $1		use UDB

# handle local UUCP connections in the Berkeley.EDU domain
R$+<@cnmat.$D . >		$#uucp$@cnmat$:$1
R$+<@cnmat.CS.$D . >		$#uucp$@cnmat$:$1
R$+<@craig.$D . >		$#uucp$@craig$:$1
R$+<@craig.CS.$D . >		$#uucp$@craig$:$1
