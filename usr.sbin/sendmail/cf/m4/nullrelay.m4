divert(-1)
#
# Copyright (c) 1983, 1995 Eric P. Allman
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
divert(0)

VERSIONID(`@(#)nullrelay.m4	8.12 (Berkeley) 10/12/96')

#
#  This configuration applies only to relay-only hosts.  They send
#  all mail to a hub without consideration of the address syntax
#  or semantics, except for adding the hub qualification to the
#  addresses.
#
#	This is based on a prototype done by Bryan Costales of ICSI.
#

######################################################################
######################################################################
#####
#####			REWRITING RULES
#####
######################################################################
######################################################################

###########################################
###  Rulset 3 -- Name Canonicalization  ###
###########################################
S3

# handle null input
R$@			$@ <@>

# strip group: syntax (not inside angle brackets!) and trailing semicolon
R$*			$: $1 <@>			mark addresses
R$* < $* > $* <@>	$: $1 < $2 > $3			unmark <addr>
R$* :: $* <@>		$: $1 :: $2			unmark node::addr
R:`include': $* <@>	$: :`include': $1			unmark :`include':...
R$* : $* <@>		$: $2				strip colon if marked
R$* <@>			$: $1				unmark
R$* ;			$: $1				strip trailing semi

# null input now results from list:; syntax
R$@			$@ :; <@>

# basic textual canonicalization -- note RFC733 heuristic here
R$*			$: < $1 >		housekeeping <>
R$+ < $* >		   < $2 >		strip excess on left
R< $* > $+		   < $1 >		strip excess on right
R<>			$@ < @ >		MAIL FROM:<> case
R< $+ >			$: $1			remove housekeeping <>

ifdef(`_NO_CANONIFY_', `dnl',
`# eliminate local host if present
R@ $=w $=: $+		$@ @ $M $2 $3			@thishost ...
R@ $+			$@ @ $1				@somewhere ...

R$=E @ $=w		$@ $1 @ $2			leave exposed
R$+ @ $=w		$@ $1 @ $M			...@thishost
R$+ @ $+		$@ $1 @ $2			...@somewhere

R$=w ! $=E		$@ $2 @ $1			leave exposed
R$=w ! $+		$@ $2 @ $M			thishost!...
R$+ ! $+		$@ $1 ! $2 @ $M			somewhere ! ...

R$=E % $=w		$@ $1 @ $2			leave exposed
R$+ % $=w		$@ $1 @ $M			...%thishost
R$+ % $+		$@ $1 @ $2			...%somewhere

R$=E			$@ $1 @ $j			leave exposed
R$+			$@ $1 @ $M			unadorned user')


######################################
###   Ruleset 0 -- Parse Address   ###
######################################

S0

R$*:;<@>		$#error $@ USAGE $: "list:; syntax illegal for recipient addresses"

# pass everything else to a relay host
R$*			$#_RELAY_ $@ $H $: $1


##################################################
###  Ruleset 4 -- Final Output Post-rewriting  ###
##################################################
S4

R$* <@>			$@				handle <> and list:;

# strip trailing dot off before passing to nullclient relay
R$* @ $+ .		$1 @ $2

#
######################################################################
######################################################################
#####
`#####			MAILER DEFINITIONS'
#####
######################################################################
######################################################################
undivert(7)dnl
