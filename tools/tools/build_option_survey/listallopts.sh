#!/bin/sh
#
# $FreeBSD: src/tools/tools/build_option_survey/listallopts.sh,v 1.1.14.1 2010/12/21 17:10:29 kensmith Exp $
#
# This file is in the public domain


set -e

T=/tmp/_$$

cd /usr/src
make showconfig __MAKE_CONF=/dev/null SRCCONF=/dev/null |
	sort |
	sed '
		s/^MK_//
		s/=//
	' | awk '
	$2 == "yes"	{ printf "WITHOUT_%s\n", $1 }
	$2 == "no"	{ printf "WITH_%s\n", $1 }
	'

