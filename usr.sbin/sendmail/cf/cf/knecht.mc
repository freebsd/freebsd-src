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
#  This is specific to Eric's home machine.
#

divert(0)dnl
VERSIONID(`@(#)knecht.mc	8.15 (Berkeley) 10/20/97')
OSTYPE(bsd4.4)dnl
DOMAIN(generic)dnl
define(`confFORWARD_PATH', `$z/.forward.$w:$z/.forward+$h:$z/.forward')dnl
define(`confDEF_USER_ID', `mailnull')dnl
define(`confHOST_STATUS_DIRECTORY', `.hoststat')dnl
define(`confTO_ICONNECT', `10s')dnl
define(`confCOPY_ERRORS_TO', `Postmaster')dnl
define(`confTO_QUEUEWARN', `8h')dnl
define(`confPRIVACY_FLAGS', ``authwarnings,noexpn,novrfy'')dnl
define(`LOCAL_MAILER_FLAGS', `rmn9P')dnl
FEATURE(virtusertable)dnl
MAILER(local)dnl
MAILER(smtp)dnl

LOCAL_CONFIG
# domains that are not us but which we will relay
FR-o /etc/sendmail.cR

# domain override table to accept unresolvable/reject resolvable domains
Kdomaincheck hash -o /etc/domaincheck


LOCAL_RULESETS

######################################################################
###  LookUpDomain -- search for domain in domaincheck database
###
###	Parameters:
###		<$1> -- key (domain name)
###		<$2> -- default (what to return if not found in db)
###		<$3> -- passthru (additional data passed through)
######################################################################

SLookUpDomain
R<$+> <$+> <$*>		$: < $( domaincheck $1 $: ? $) > <$1> <$2> <$3>
R<OK> <$+> <$+> <$*>	$@ <OK> < $3 >
R<?> <$+.$+> <$+> <$*>	$@ $>LookUpDomain <. $2> <$3> <$4>
R<?> <$+> <$+> <$*>	$@ <$2> <$3>
R<$+> $*		$#error $: $1


######################################################################
###  LookUpAddress -- search for host address in domaincheck database
###
###	Parameters:
###		<$1> -- key (dot quadded host address)
###		<$2> -- default (what to return if not found in db)
###		<$3> -- passthru (additional data passed through)
######################################################################

SLookUpAddress
R<$+> <$+> <$*>		$: < $( domaincheck $1 $: ? $) > <$1> <$2> <$3>
R<OK> <$+> <$+> <$*>	$@ <OK> < $3 >
R<?> <$+.$-> <$+> <$*>	$@ $>LookUpAddress <$1> <$3> <$4>
R<?> <$+> <$+> <$*>	$@ <$2> <$3>
R<$+> $*		$#error $: $1

######################################################################
###  check_relay
######################################################################

Scheck_relay
R$+ $| $+		$: $>LookUpDomain < $1 > <?> < $2 >
R<?> < $+ >		$: $>LookUpAddress < $1 > <OK> <>

######################################################################
###  check_mail
######################################################################

Scheck_mail
R<>			$@ <OK>
R$*			$: <?> $>Parse0 $>3 $1		make domain canonical
R<?> $* < @ $+ . > $*	$: <OK> $1 < @ $2 > $3		pick default tag
R<?> $* < @ $+ > $*	$: <FAIL> $1 < @ $2 > $3	... OK or FAIL
R<$+> $* < @ $+ > $*	$: $>LookUpDomain <$3> <$1> <>
R<OK> $*		$@ <OK>
R<FAIL> $*		$#error $: 451 Sender domain must resolve

# handle case of no @domain on address
R<?> $*			$: < ? $&{client_name} > $1
R<?> $*			$@ <OK>				...local unqualed ok
R<? $+> $*		$#error $: 550 Domain name required
							...remote is not
R<$+> $*		$#error $: $1			error from domaincheck

######################################################################
###  check_rcpt
######################################################################

Scheck_rcpt
# anything terminating locally is ok
R$*			$: $>Parse0 $>3 $1		strip local crud
R$+ < @ $=w . >		$@ OK
R$+ < @ $* $=R . >	$@ OK

# anything originating locally is ok
R$*			$: $(dequote "" $&{client_name} $)
R$=w			$@ OK
R$=R			$@ OK
R$@			$@ OK

# anything else is bogus
R$*			$#error $: "550 Relaying Denied"
