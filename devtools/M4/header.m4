#
# Copyright (c) 1998, 1999, 2014 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: header.m4,v 8.29 2014-01-26 20:40:45 gshapiro Exp $
#
changecom(^A)
undefine(`format')
undefine(`hpux')
undefine(`unix')
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
define(`confLIBSEARCHPATH', `/lib /usr/lib /usr/shlib')
define(`confSHAREDLIB_EXT', `.so')
define(`confSITECONFIG', `site.config')
define(`confBUILDBIN', `${SRCDIR}/devtools/bin')
define(`confRANLIB', `echo')
define(`PUSHDIVERT', `pushdef(`__D__', divnum)divert($1)')
define(`POPDIVERT', `divert(__D__)popdef(`__D__')')
define(`APPENDDEF', `define(`$1', ifdef(`$1', `$1 $2', `$2'))')
define(`PREPENDDEF', `define(`$1', ifdef(`$1', `$2 $1', `$2'))')
