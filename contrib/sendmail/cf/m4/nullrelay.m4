divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983, 1995 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
divert(0)

VERSIONID(`@(#)nullrelay.m4	8.19 (Berkeley) 5/19/1998')

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
R$* ;			   $1				strip trailing semi
R$* < $* ; >		   $1 < $2 >			bogus bracketed semi

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

R$*:;<@>		$#error $@ USAGE $: "List:; syntax illegal for recipient addresses"

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
