#!/bin/sh

# Copyright (c) 1998-2001 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#       $Id: find_m4.sh,v 8.14 2013-11-22 20:51:24 ca Exp $
#

# Try to find a working M4 program.
# If $M4 is already set, we use it, otherwise we prefer GNU m4.

EX_UNAVAILABLE=69

test="ifdef(\`pushdef', \`',
\`errprint(\`You need a newer version of M4, at least as new as System V or GNU')
include(NoSuchFile)')
define(\`BadNumber', \`10')
ifdef(\`BadNumber', \`',
\`errprint(\`This version of m4 is broken: trailing zero problem')
include(NoSuchFile)')
define(\`LongList', \` assert.c debug.c exc.c heap.c match.c rpool.c strdup.c strerror.c strl.c clrerr.c fclose.c feof.c ferror.c fflush.c fget.c fpos.c findfp.c flags.c fopen.c fprintf.c fpurge.c fput.c fread.c fscanf.c fseek.c fvwrite.c fwalk.c fwrite.c get.c makebuf.c put.c refill.c rewind.c rget.c setvbuf.c smstdio.c snprintf.c sscanf.c stdio.c strio.c syslogio.c ungetc.c vasprintf.c vfprintf.c vfscanf.c vprintf.c vsnprintf.c vsprintf.c vsscanf.c wbuf.c wsetup.c stringf.c xtrap.c strto.c test.c path.c strcasecmp.c signal.c clock.c config.c shm.c ')
define(\`SameList', \`substr(LongList, 0, index(LongList, \`.'))\`'substr(LongList, index(LongList, \`.'))')
ifelse(len(LongList), len(SameList), \`',
\`errprint(\`This version of m4 is broken: length problem')
include(NoSuchFile)')"

if [ "$M4" ]
then
	err="`(echo "$test" | $M4) 2>&1 >/dev/null`"
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
				err="`(echo "$test" | $dir/$m4) 2>&1 >/dev/null`"
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
