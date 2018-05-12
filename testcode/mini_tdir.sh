# tdir that only exes the files.
args="../.."
if test "$1" = "-a"; then
	args=$2
	shift
	shift
fi
	
if test "$1" = "clean"; then
	echo "rm -f result.* .done* .tdir.var.master .tdir.var.test"
	rm -f result.* .done* .tdir.var.master .tdir.var.test
	exit 0
fi
if test "$1" = "fake"; then
	echo "minitdir fake $2"
	echo "fake" > .done-`basename $2 .tdir`
	exit 0
fi
if test "$1" = "-f" && test "$2" = "report"; then
	echo "Minitdir Long Report"
	pass=0
	fail=0
	skip=0
	echo "   STATUS    ELAPSED TESTNAME TESTDESCRIPTION"
	for result in *.tdir; do
		name=`basename $result .tdir`
		timelen="     "
		desc=""
		if test -f "result.$name"; then
			timestart=`grep ^DateRunStart: "result.$name" | sed -e 's/DateRunStart: //'`
			timeend=`grep ^DateRunEnd: "result.$name" | sed -e 's/DateRunEnd: //'`
			timesec=`expr $timeend - $timestart`
			timelen=`printf %4ds $timesec`
			if test $? -ne 0; then
				timelen="$timesec""s"
			fi
			desc=`grep ^Description: "result.$name" | sed -e 's/Description: //'`
		fi
		if test -f ".done-$name"; then
			if test "$1" != "-q"; then
				echo "** PASSED ** $timelen $name: $desc"
				pass=`expr $pass + 1`
			fi
		else
			if test -f "result.$name"; then
				echo "!! FAILED !! $timelen $name: $desc"
				fail=`expr $fail + 1`
			else
				echo ".> SKIPPED<< $timelen $name: $desc"
				skip=`expr $skip + 1`
			fi
		fi
	done
	echo ""
	if test "$skip" = "0"; then
		echo "$pass pass, $fail fail"
	else
		echo "$pass pass, $fail fail, $skip skip"
	fi
	echo ""
	exit 0
fi
if test "$1" = "report" || test "$2" = "report"; then
	echo "Minitdir Report"
	for result in *.tdir; do
		name=`basename $result .tdir`
		if test -f ".done-$name"; then
			if test "$1" != "-q"; then
				echo "** PASSED ** : $name"
			fi
		else
			if test -f "result.$name"; then
				echo "!! FAILED !! : $name"
			else
				echo ">> SKIPPED<< : $name"
			fi
		fi
	done
	exit 0
fi

if test "$1" != 'exe'; then
	# usage
	echo "mini tdir. Reduced functionality for old shells."
	echo "	tdir exe <file>"
	echo "	tdir fake <file>"
	echo "	tdir clean"
	echo "	tdir [-q|-f] report"
	exit 1
fi
shift

# do not execute if the disk is too full
#DISKLIMIT=100000
# This check is not portable (to Solaris 10).
#avail=`df . | tail -1 | awk '{print $4}'`
#if test "$avail" -lt "$DISKLIMIT"; then
	#echo "minitdir: The disk is too full! Only $avail."
	#exit 1
#fi

name=`basename $1 .tdir`
dir=$name.$$
result=result.$name
done=.done-$name
success="no"
if test -x "`which bash`"; then
	shell="bash"
else
	shell="sh"
fi

# check already done
if test -f .done-$name; then
	echo "minitdir .done-$name exists. skip test."
	exit 0
fi

# Copy
echo "minitdir copy $1 to $dir"
mkdir $dir
cp -a $name.tdir/* $dir/
cd $dir

# EXE
echo "minitdir exe $name" > $result
grep "Description:" $name.dsc >> $result 2>&1
echo "DateRunStart: "`date "+%s" 2>/dev/null` >> $result
if test -f $name.pre; then
	echo "minitdir exe $name.pre"
	echo "minitdir exe $name.pre" >> $result
	$shell $name.pre $args >> $result
	if test $? -ne 0; then
		echo "Warning: $name.pre did not exit successfully"
	fi
fi
if test -f $name.test; then
	echo "minitdir exe $name.test"
	echo "minitdir exe $name.test" >> $result
	$shell $name.test $args >>$result 2>&1
	if test $? -ne 0; then
		echo "$name: FAILED" >> $result
		echo "$name: FAILED"
		success="no"
	else
		echo "$name: PASSED" >> $result
		echo "$name: PASSED" > ../.done-$name
		echo "$name: PASSED"
		success="yes"
	fi
fi
if test -f $name.post; then
	echo "minitdir exe $name.post"
	echo "minitdir exe $name.post" >> $result
	$shell $name.post $args >> $result
	if test $? -ne 0; then
		echo "Warning: $name.post did not exit successfully"
	fi
fi
echo "DateRunEnd: "`date "+%s" 2>/dev/null` >> $result

mv $result ..
cd ..
rm -rf $dir
# compat for windows where deletion may not succeed initially (files locked
# by processes that still have to exit).
if test $? -eq 1; then
	echo "minitdir waiting for processes to terminate"
	sleep 2 # some time to exit, and try again
	rm -rf $dir
fi
