#!/bin/sh

# $FreeBSD$

COUNT=0
TMPDIR=$(pwd)/work
if [ $? -ne 0 ]; then
        echo "$0: Can't create temp dir, exiting..."
        exit 1
fi

# Begin an individual test
begin()
{
	COUNT=`expr $COUNT + 1`
	OK=1
	NAME="$1"
}

# End an individual test
end()
{
	local message

	if [ $OK = 1 ]
	then
		message='ok '
	else
		message='not ok '
	fi

	message="$message $COUNT - $NAME"
	if [ -n "$TODO" ]
	then
		message="$message # TODO $TODO"
	fi

	echo "$message"
}

# Make a file that can later be verified
mkf()
{
	CN=`basename $1`
	echo "$CN-$CN" >$1
}

# Verify that the file specified is correct
ckf()
{
	if [ -f $2 ] && echo "$1-$1" | diff - $2 >/dev/null
	then
		ok
	else
		notok
	fi
}

# Check that a file exists
ckfe()
{
	if [ -f $1 ]
	then
		ok
	else
		notok
	fi
}

# Verify that the specified file does not exist
# (is not there)
cknt()
{
	if [ -r $1 ]
	then
		notok
	else
		ok
	fi
}

# Check if a file is there, depending of if it's supposed to or not -
# basically how many log files we are supposed to keep vs. how many we
# actually keep.
ckntfe()
{
	curcnt=$1
	keepcnt=$2
	f=$3

	if [ $curcnt -le $keepcnt ]
	then
		#echo Assuming file there
		ckfe $f
	else
		#echo Assuming file NOT there
		cknt $f
	fi
}



# A part of a test succeeds
ok()
{
	:
}

# A part of a test fails
notok()
{
	OK=0
}

# Verify that the exit code passed is for unsuccessful termination
ckfail()
{
	if [ $1 -gt 0 ]
	then
		ok
	else
		notok
	fi
}

# Verify that the exit code passed is for successful termination
ckok()
{
	if [ $1 -eq 0 ]
	then
		ok
	else
		notok
	fi
}

# Check that there are X files which match expr
chkfcnt()
{
	cnt=$1; shift
	if [ $cnt -eq `echo "$@" | wc -w` ]
	then
		ok
	else
		notok
	fi
}

# Check that two strings are alike
ckstr()
{
	if [ "$1" = "$2" ]
	then
		ok
	else
		notok
	fi
}

tmpdir_create()
{
	mkdir -p ${TMPDIR}/log ${TMPDIR}/alog
	cd ${TMPDIR}/log
}

tmpdir_clean()
{
	cd ${TMPDIR}
	rm -rf "${TMPDIR}/log" "${TMPDIR}/alog" newsyslog.conf
}

run_newsyslog()
{

	newsyslog -f ../newsyslog.conf -F -r "$@"
}

tests_normal_rotate() {
	ext="$1"
	dir="$2"

	if [ -n "$dir" ]; then
		newsyslog_args=" -a ${dir}"
		name_postfix="${ext} archive dir"
	else
		newsyslog_args=""
		name_postfix="${ext}"
	fi

	tmpdir_create

	begin "create file ${name_postfix}" -newdir
	run_newsyslog -C
	ckfe $LOGFNAME
	cknt ${dir}${LOGFNAME}.0${ext}
	end

	begin "rotate normal 1 ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckfe ${dir}${LOGFNAME}.0${ext}
	cknt ${dir}${LOGFNAME}.1${ext}
	end

	begin "rotate normal 2 ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckfe ${dir}${LOGFNAME}.0${ext}
	ckfe ${dir}${LOGFNAME}.1${ext}
	cknt ${dir}${LOGFNAME}.2${ext}
	end

	begin "rotate normal 3 ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckfe ${dir}${LOGFNAME}.0${ext}
	ckfe ${dir}${LOGFNAME}.1${ext}
	ckfe ${dir}${LOGFNAME}.2${ext}
	cknt ${dir}${LOGFNAME}.3${ext}
	end

	begin "rotate normal 4 ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckfe ${dir}${LOGFNAME}.0${ext}
	ckfe ${dir}${LOGFNAME}.1${ext}
	ckfe ${dir}${LOGFNAME}.2${ext}
	cknt ${dir}${LOGFNAME}.4${ext}
	end

	begin "rotate normal 5 ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckfe ${dir}${LOGFNAME}.0${ext}
	ckfe ${dir}${LOGFNAME}.1${ext}
	ckfe ${dir}${LOGFNAME}.2${ext}
	cknt ${dir}${LOGFNAME}.4${ext}
	end

	# Wait a bit so we can see if the noaction test rotates files
	sleep 1.1

	begin "noaction ${name_postfix}"
	ofiles=`ls -Tl ${dir}${LOGFNAME}.*${ext} | tr -d '\n'`
	run_newsyslog ${newsyslog_args} -n >/dev/null
	ckfe ${LOGFNAME}
	ckstr "$ofiles" "`ls -lT ${dir}${LOGFNAME}.*${ext} | tr -d '\n'`"
	end

	tmpdir_clean
}

tests_normal_rotate_keepn() {
	cnt="$1"
	ext="$2"
	dir="$3"

	if [ -n "$dir" ]; then
		newsyslog_args=" -a ${dir}"
		name_postfix="${ext} archive dir"
	else
		newsyslog_args=""
		name_postfix="${ext}"
	fi

	tmpdir_create

	begin "create file ${name_postfix}" -newdir
	run_newsyslog -C
	ckfe $LOGFNAME
	cknt ${dir}${LOGFNAME}.0${ext}
	end

	begin "rotate normal 1 cnt=$cnt ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckntfe 1 $cnt ${dir}${LOGFNAME}.0${ext}
	cknt ${dir}${LOGFNAME}.1${ext}
	end

	begin "rotate normal 2 cnt=$cnt ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckntfe 1 $cnt ${dir}${LOGFNAME}.0${ext}
	ckntfe 2 $cnt ${dir}${LOGFNAME}.1${ext}
	cknt ${dir}${LOGFNAME}.2${ext}
	end

	begin "rotate normal 3 cnt=$cnt ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckntfe 1 $cnt ${dir}${LOGFNAME}.0${ext}
	ckntfe 2 $cnt ${dir}${LOGFNAME}.1${ext}
	ckntfe 3 $cnt ${dir}${LOGFNAME}.2${ext}
	cknt ${dir}${LOGFNAME}.3${ext}
	end

	begin "rotate normal 3 cnt=$cnt ${name_postfix}"
	run_newsyslog $newsyslog_args
	ckfe ${LOGFNAME}
	ckntfe 1 $cnt ${dir}${LOGFNAME}.0${ext}
	ckntfe 2 $cnt ${dir}${LOGFNAME}.1${ext}
	ckntfe 3 $cnt ${dir}${LOGFNAME}.2${ext}
	ckntfe 4 $cnt ${dir}${LOGFNAME}.3${ext}
	cknt ${dir}${LOGFNAME}.4${ext}
	end

	# Wait a bit so we can see if the noaction test rotates files
	sleep 1.1

	begin "noaction ${name_postfix}"
	osum=`md5 ${dir}${LOGFNAME} | tr -d '\n'`
	run_newsyslog ${newsyslog_args} -n >/dev/null
	ckfe ${LOGFNAME}
	ckstr "$osum" "`md5 ${dir}${LOGFNAME} | tr -d '\n'`"
	end

	tmpdir_clean
}

tests_time_rotate() {
	ext="$1"
	dir="$2"

	if [ -n "$dir" ]; then
		newsyslog_args="-t DEFAULT -a ${dir}"
		name_postfix="${ext} archive dir"
	else
		newsyslog_args="-t DEFAULT"
		name_postfix="${ext}"
	fi

	tmpdir_create

	begin "create file ${name_postfix}" -newdir
	run_newsyslog -C ${newsyslog_args}
	ckfe ${LOGFNAME}
	end

	begin "rotate time 1 ${name_postfix}"
	run_newsyslog ${newsyslog_args}
	ckfe ${LOGFNAME}
	chkfcnt 1 ${dir}${LOGFNAME}.*${ext}
	end

	sleep 1.1

	(
	TODO="rotate time 2-4 fail today; bug 212160"

	begin "rotate time 2 ${name_postfix}"
	run_newsyslog ${newsyslog_args}
	ckfe ${LOGFNAME}
	chkfcnt 2 ${dir}${LOGFNAME}.*${ext}
	end

	sleep 1.1

	begin "rotate time 3 ${name_postfix}"
	run_newsyslog ${newsyslog_args}
	ckfe ${LOGFNAME}
	chkfcnt 3 ${dir}${LOGFNAME}.*${ext}
	end

	sleep 1.1

	begin "rotate time 4 ${name_postfix}"
	run_newsyslog ${newsyslog_args}
	ckfe ${LOGFNAME}
	chkfcnt 3 ${dir}${LOGFNAME}.*${ext}
	end
	)

	begin "noaction ${name_postfix}"
	ofiles=`ls -1 ${dir}${LOGFNAME}.*${ext} | tr -d '\n'`
	run_newsyslog ${newsyslog_args} -n >/dev/null
	ckfe ${LOGFNAME}
	ckstr "$ofiles" "`ls -1 ${dir}${LOGFNAME}.*${ext} | tr -d '\n'`"
	end

	tmpdir_clean
}

echo 1..126
mkdir -p ${TMPDIR}
cd ${TMPDIR}

LOGFNAME=foo.log
LOGFPATH=${TMPDIR}/log/${LOGFNAME}

# Normal, no archive dir, keep X files
echo "$LOGFPATH	640  0	   *	@T00  NC" > newsyslog.conf
tests_normal_rotate_keepn 0

echo "$LOGFPATH	640  1	   *	@T00  NC" > newsyslog.conf
tests_normal_rotate_keepn 1

echo "$LOGFPATH	640  2	   *	@T00  NC" > newsyslog.conf
tests_normal_rotate_keepn 2

echo "$LOGFPATH	640  3	   *	@T00  NC" > newsyslog.conf
tests_normal_rotate_keepn 3

# Normal, no archive dir, keep X files, gz
echo "$LOGFPATH	640  0	   *	@T00  NCZ" > newsyslog.conf
tests_normal_rotate_keepn 0 ".gz"

echo "$LOGFPATH	640  1	   *	@T00  NCZ" > newsyslog.conf
tests_normal_rotate_keepn 1 ".gz"

echo "$LOGFPATH	640  2	   *	@T00  NCZ" > newsyslog.conf
tests_normal_rotate_keepn 2 ".gz"

echo "$LOGFPATH	640  3	   *	@T00  NCZ" > newsyslog.conf
tests_normal_rotate_keepn 3 ".gz"

# Normal, no archive dir
echo "$LOGFPATH	640  3	   *	@T00  NC" > newsyslog.conf
tests_normal_rotate

echo "$LOGFPATH	640  3	   *	@T00  NCZ" > newsyslog.conf
tests_normal_rotate ".gz"

echo "$LOGFPATH	640  3	   *	@T00  NCJ" > newsyslog.conf
tests_normal_rotate ".bz2"

# Normal, archive dir
echo "$LOGFPATH	640  3	   *	@T00  NC" > newsyslog.conf
tests_normal_rotate "" "${TMPDIR}/alog/"

echo "$LOGFPATH	640  3	   *	@T00  NCZ" > newsyslog.conf
tests_normal_rotate ".gz" "${TMPDIR}/alog/"

echo "$LOGFPATH	640  3	   *	@T00  NCJ" > newsyslog.conf
tests_normal_rotate ".bz2" "${TMPDIR}/alog/"

# Time based, no archive dir
echo "$LOGFPATH	640  3	   *	@T00  NC" > newsyslog.conf
tests_time_rotate

echo "$LOGFPATH	640  3	   *	@T00  NCZ" > newsyslog.conf
tests_time_rotate "gz" ""

echo "$LOGFPATH	640  3	   *	@T00  NCJ" > newsyslog.conf
tests_time_rotate "bz2" ""

# Time based, archive dir
echo "$LOGFPATH	640  3	   *	@T00  NC" > newsyslog.conf
tests_time_rotate "" "${TMPDIR}/alog/"

echo "$LOGFPATH	640  3	   *	@T00  NCZ" > newsyslog.conf
tests_time_rotate "gz" "${TMPDIR}/alog/"

echo "$LOGFPATH	640  3	   *	@T00  NCJ" > newsyslog.conf
tests_time_rotate "bz2" "${TMPDIR}/alog/"

rm -rf "${TMPDIR}"
