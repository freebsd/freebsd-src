#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	@(#)header.m4	8.14	(Berkeley)	5/19/1998
#
changecom(^A)
undefine(`format')
undefine(`hpux')
ifdef(`pushdef', `',
	`errprint(`You need a newer version of M4, at least as new as
System V or GNU')
	include(NoSuchFile)')
define(`confABI', `')
define(`confCC', `cc')
define(`confSHELL', `/bin/sh')
define(`confBEFORE', `')
define(`confLIBDIRS', `')
define(`confINCDIRS', `')
define(`confLIBSEARCH', `db bind resolv 44bsd')
define(`confSITECONFIG', `site.config')
define(`confBUILDBIN', `../../BuildTools/bin')
define(`PUSHDIVERT', `pushdef(`__D__', divnum)divert($1)')
define(`POPDIVERT', `divert(__D__)popdef(`__D__')')
define(`APPENDDEF', `define(`$1', ifdef(`$1', `$1 $2', `$2'))')
define(`PREPENDDEF', `define(`$1', ifdef(`$1', `$2 $1', `$2'))')
