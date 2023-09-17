#!/bin/sh

#
# Copyright (c) 2008-2009, 2012-13 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Run all the scripts in stress2/misc.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Log and config files:
sdir=/tmp/stress2.d
mkdir -p $sdir
allconfig=$sdir/`hostname`	# config file
allfaillog=$sdir/fail		# Tests that failed
alllast=$sdir/last		# Last test run
alllist=$sdir/list		# -o list
alllog=$sdir/log		# Tests run
alloutput=$sdir/output		# Output from current test
allexcess=$sdir/excessive	# Tests with excessive runtime
allelapsed=$sdir/elapsed	# Test runtime
alllocal=$sdir/all.exclude	# Local exclude list
loops=0				# Times to run the tests
# Get kernel config + revision
rev=`uname -a | awk '{print $7}' | sed 's/://'`
rev="`uname -a | sed 's#.*/compile/##; s/ .*//'` $rev"

args=`getopt acl:m:no "$@"`
[ $? -ne 0 ] &&
    echo "Usage $0 [-a] [-c] [-l <val>] [-m <min.>] [-n] [-o] [<tests>]" &&
    exit 1
set -- $args
for i; do
	case "$i" in
	-a)	all=1		# Run all tests
		echo "Note: including known problem tests."
		shift
		;;
	-c)	rm -f $alllast	# Clear last know test
		rm -f $alllist
		shift
		;;
	-l)	loops=$2	# Number of time to run
		shift; shift
		;;
	-m)	minutes=$(($2 * 60))	# Run for minutes
		shift; shift
		;;
	-n)	noshuffle=1	# Do not shuffle the list of tests
		shift		# Resume test after last test
		;;
	-o)	loops=1		# Only run once
		shift
		;;
	--)
		shift
		break
		;;
	esac
done

export allconfig
if [ !  -f $allconfig ]; then
	echo "Creating local configuration file: $allconfig."
	../tools/setup.sh || exit 1
fi

. ../default.cfg

# Sanity checks
minspace=$((1024 * 1024)) # in k
[ -d `dirname "$diskimage"` ] ||
    { echo "diskimage dir: $diskimage not found"; exit 1; }
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print $4}'` -lt \
    $minspace ] &&
    echo "Warn: Not enough disk space on `dirname $diskimage` " \
	"for \$diskimage"
[ ! -d $(dirname $RUNDIR) ] &&
    echo "No such \$RUNDIR \"`dirname $RUNDIR`\"" &&
    exit 1
[ `sysctl -n hw.physmem` -le $((3 * 1024 * 1024 * 1024)) ] &&
	echo "Warn: Small RAM size for stress tests `sysctl -n hw.physmem`"
[ `df -k $(dirname $RUNDIR) | tail -1 | awk '{print $4}'` -lt \
    $minspace ] &&
    echo "Warn: Not enough disk space on `dirname $RUNDIR` for \$RUNDIR"
id $testuser > /dev/null 2>&1 ||
    { echo "\$testuser \"$testuser\" not found."; exit 1; }
probe=`dirname $RUNDIR`/probe
su $testuser -c "touch $probe" > /dev/null 2>&1
[ -f $probe ] && rm $probe ||
    { echo "No write access to `dirname $RUNDIR`."; exit 1; }
[ `swapinfo | wc -l` -eq 1 ] &&
    echo "Consider adding a swap disk. Many tests rely on this."
mount | grep -wq $mntpoint &&
    echo "\$mntpoint ($mntpoint) is already in use" && exit 1
[ -x ../testcases/run/run ] ||
	(cd ..; make)
ping -c 2 -t 2 $BLASTHOST > /dev/null 2>&1 ||
    { echo "Note: Can not ping \$BLASTHOST: $BLASTHOST"; }
echo "$loops" | grep -Eq "^[0-9]+$" ||
    { echo "The -l argument must be a positive number"; exit 1; }
[ `grep "^[a-zA-Z].*\.sh" $alllocal 2>/dev/null | wc -l` -ne 0 ] &&
    echo "Using $alllocal"

find `dirname $alllast` -maxdepth 1 -name $alllast -mtime +12h -delete
touch $alllast $alllog
chmod 640 $alllast $alllog
find ../testcases -perm -1 \( -name "*.debug" -o -name "*.full" \) -delete
tail -2000 $alllog > ${alllog}.new; mv ${alllog}.new $alllog
touch $allelapsed
tail -20000 $allelapsed > ${allelapsed}.new; mv ${allelapsed}.new $allelapsed

console=/dev/console
printf "\r\n" > $console &
pid=$!
sleep 1
kill -0 $pid > /dev/null 2>&1 &&
{ console=/dev/null; kill -9 $pid; }
while pgrep -q fsck; do sleep 10; done

status() {
	local s2 r

	s2=`date +%s`
	r=$(echo "elapsed $(((s2 - s1) / 86400)) day(s)," \
	    "`date -u -j -f '%s' '+%H:%M.%S' $((s2 - s1))`")
	printf "`date '+%Y%m%d %T'` all.sh done, $r\n"
	printf "`date '+%Y%m%d %T'` all.sh done, $r\r\n" > $console
}

intr() {
	printf "\nExit all.sh\n"
	./cleanup.sh
	exit 1
}
trap status EXIT
trap intr INT

[ -f all.debug.inc ] && . all.debug.inc
s1=`date +%s`
while true; do
	exclude=`cat all.exclude $alllocal 2>/dev/null | sed '/^#/d' |
	    grep "^[a-zA-Z].*\.sh" | awk '{print $1}'`
	list=`echo *.sh`
	[ $# -ne 0 ] && list=$*
	list=`echo $list |
	     sed  "s/[[:<:]]all\.sh[[:>:]]//g;\
	           s/[[:<:]]cleanup\.sh[[:>:]]//g"`

	if [ -n "$noshuffle" -a $# -eq 0 ]; then
		last=`cat $alllast`
		if [ -n "$last" ]; then
			last=`basename $last`
			l=`cat "$alllist" | sed "s/.*$last//"`
			[ -z "$l" ] && l=$list	# start over
			list=$l
			echo "Last test was $last,"\
			    "resuming test at" \
			    "`echo "$list" | awk '{print $1}'`"
		fi
	fi
	[ -n "$noshuffle" ] ||
	    list=`echo $list | tr ' ' '\n' | sort -R |
	        tr '\n' ' '`

	lst=""
	for i in $list; do
		[ -z "$all" ] && echo $exclude | grep -qw `basename $i` &&
		    continue
		lst="$lst $i"
	done
	[ -z "$lst" ] && exit
	echo "$lst" > $alllist

	pgrep -fq vmstat.sh ||
	    daemon ../tools/vmstat.sh > /tmp/stress2.d/vmstat 2>&1

	n1=0
	n2=`echo $lst | wc -w | sed 's/ //g'`
	for i in $lst; do
		i=`basename $i`
		[ ! -f ./$i ] && { echo "No such file ./$i"; continue; }
		n1=$((n1 + 1))
		echo $i > $alllast
		./cleanup.sh || exit 1
		ts=`date '+%Y%m%d %T'`
		echo "$ts all: $i"
		printf "$ts all ($n1/$n2): $i\n" >> $alllog
		printf "$ts all ($n1/$n2): $i\r\n" > $console
		logger "Starting stress2 test all.sh: $i"
		[ $all_debug ] && pre_debug
		[ -f $i ] || loops=1	# break
		sync; sleep .5; sync; sleep .5
		grep -E "^USE_TIMEOUT=1" $i && TIMEOUT_ONE=1 ||
		    unset TIMEOUT_ONE
		start=`date '+%s'`
		(
			if [ $USE_TIMEOUT ] || [ $TIMEOUT_ONE ]; then
				timeout -k 1m 1h ./$i
			else
				./$i
			fi
			e=$?
			[ $e -ne 0 ] &&
			    echo "FAIL $i exit code $e"
		) 2>&1 | tee $alloutput
		ts=`date '+%Y%m%d %T'`
		grep -qw FAIL $alloutput &&
		    echo "$ts $rev $i" >> $allfaillog &&
		    logger "stress2 test $i failed"
		grep -qw FATAL $alloutput && exit $e
		rm -f $alloutput
		printf "$ts $rev $i $((`date '+%s'` - start))\n" >> \
		    $allelapsed
		[ -f ../tools/ministat.sh ] &&
		    ../tools/ministat.sh $allelapsed $i
		[ $((`date '+%s'` - start)) -gt 1980 ] &&
		    printf "$ts $rev $i %d min\n" \
		        $(((`date '+%s'` - start) / 60)) >> $allexcess
		while pgrep -q "^swap$"; do
			echo "swap still running"
			sleep 2
		done
		[ $USE_SWAPOFF ] && { swapoff -a; swapon -a; }
		[ $all_debug ] && post_debug
		[ $minutes ] && [ $((`date +%s` - s1)) -ge $minutes ] &&
		    break 2
	done
	[ $((loops -= 1)) -eq 0 ] && break
done
[ -x ../tools/fail.sh ] && ../tools/fail.sh
find /tmp . -name "*.core" -mtime -2 -maxdepth 2 -ls 2>/dev/null
