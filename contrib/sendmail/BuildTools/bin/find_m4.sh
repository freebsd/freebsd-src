#!/bin/sh

# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#       @(#)find_m4.sh	8.4 (Berkeley) 5/19/1998
#

# Try to find a working M4 program.
# If $M4 is already set, we use it, otherwise we prefer GNU m4.

EX_UNAVAILABLE=69

test="ifdef(\`pushdef', \`',
\`errprint(\`You need a newer version of M4, at least as new as System V or GNU')
include(NoSuchFile)')
define(\`BadNumber', \`10')
ifdef(\`BadNumber', \`', \`errprint(\`This version of m4 is broken')')"

if [ "$M4" ]
then
	err=`(echo "$test" | $M4) 2>&1 >/dev/null`
	code=$?
else
	firstfound=
	ifs="$IFS"; IFS="${IFS}:"
	for m4 in gm4 gnum4 pdm4 m4
	do
		for dir in $PATH /usr/5bin /usr/ccs/bin
		do
			[ -z "$dir" ] && dir=.
			if [ -f $dir/$m4 ]
			then
				err=`(echo "$test" | $dir/$m4) 2>&1 >/dev/null`
				ret=$?
				if [ $ret -eq 0 -a "X$err" = "X" ]
				then
					M4=$dir/$m4
					code=0
					break
				else
					case "$firstfound:$err" in
					  :*version\ of*)
						firstfound=$dir/$m4
						firsterr="$err"
						firstcode=$ret
						;;
					esac
				fi
			fi
		done
		[ "$M4" ] && break
	done
	IFS="$ifs"
	if [ ! "$M4" ]
	then
		if [ "$firstfound" ]
		then
			M4=$firstfound
			err="$firsterr"
			code=$firstcode
		else
			echo "ERROR: Can not locate an M4 program" >&2
			exit $EX_UNAVAILABLE
		fi
	fi
fi
if [ $code -ne 0 ]
then
	echo "ERROR: Using M4=$M4: $err" | grep -v NoSuchFile >&2
	exit $EX_UNAVAILABLE
elif [ "X$err" != "X" ]
then
	echo "WARNING: $err" >&2
fi
echo $M4
exit 0

