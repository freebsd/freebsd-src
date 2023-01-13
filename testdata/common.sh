# common.sh - an include file for commonly used functions for test code.
# BSD licensed (see LICENSE file).
#
# Version 3
# 2011-02-23: get_pcat for PCAT, PCAT_DIFF and PCAT_PRINT defines.
# 2011-02-18: ports check on BSD,Solaris. wait_nsd_up.
# 2011-02-11: first version.
#
# include this file from a tdir script with
#   . ../common.sh
#
# overview of functions available:
# error x		: print error and exit
# info x		: print info
# test_tool_avail x	: see if program in path and complain, exit if not.
# get_ldns_testns	: set LDNS_TESTNS to executable ldns-testns
# get_ldns_notify	: set LDNS_NOTIFY to executable ldns-notify
# get_make		: set MAKE to gmake or make tool.
# get_gcc		: set cc or gcc in CC
# get_pcat		: set PCAT, PCAT_DIFF and PCAT_PRINT executables.
# set_doxygen_path	: set doxygen path
# skip_if_in_list	: set SKIP=1 if name in list and tool not available.
# get_random_port x	: get RND_PORT a sequence of free random port numbers.
# wait_server_up	: wait on logfile to see when server comes up.
# wait_ldns_testns_up   : wait for ldns-testns to come up.
# wait_unbound_up	: wait for unbound to come up.
# wait_petal_up		: wait for petal to come up.
# wait_nsd_up		: wait for nsd to come up.
# wait_server_up_or_fail: wait for server to come up or print a failure string
# skip_test x		: print message and skip test (must be called in .pre)
# kill_pid		: kill a server, make sure and wait for it to go down.
# teststep		: print the current test step in the output


# print error and exit
# $0: name of program
# $1: error to printout.
error () {
	echo "$0: error: $1" >&2
	exit 1
}

# print info
# $0: name of program
# $1: to printout.
info () {
	echo "$0: info: $1"
}

# test if 'tool' is available in path and complain otherwise.
# $1: tool
test_tool_avail () {
	if test ! -x "`which $1 2>&1`"; then
		echo No "$1" in path
		exit 1
	fi
}

# get ldns-testns tool in LDNS_TESTNS variable.
get_ldns_testns () {
	if test -x "`which ldns-testns 2>&1`"; then
		LDNS_TESTNS=ldns-testns
	else
		LDNS_TESTNS=/home/wouter/bin/ldns-testns
	fi
}

# get ldns-notify tool in LDNS_NOTIFY variable.
get_ldns_notify () {
	if test -x "`which ldns-notify 2>&1`"; then
		LDNS_NOTIFY=ldns-notify
	else
		LDNS_NOTIFY=/home/wouter/bin/ldns-notify
	fi
}

# get make tool in MAKE variable, gmake is used if present.
get_make () {
	if test -x "`which gmake 2>&1`"; then
		MAKE=gmake
	else
		MAKE=make
	fi
}

# get cc tool in CC variable, gcc is used if present.
get_gcc () {
	if test -x "`which gcc 2>&1`"; then
		CC=gcc
	else
		CC=cc
	fi
}

# get pcat, pcat-print and pcat-diff
get_pcat () {
	PCAT=`which pcat`
	PCAT_PRINT=`which pcat-print`
	PCAT_DIFF=`which pcat-diff`
}

# set SKIP=1 if the name is in list and tool is not available.
# $1: name of package to check.
# $2: list of packages that need the tool.
# #3: name of the tool required.
skip_if_in_list () {
	if echo $2 | grep $1 >/dev/null; then
		if test ! -x "`which $3 2>&1`"; then
			SKIP=1;
		fi
	fi
}

# Print a message and skip the test. Must be called in the .pre file.
# $1: message to print.
skip_test () {
	echo "$1"
	exit 3
}

# function to get a number of random port numbers.
# $1: number of random ports.
# RND_PORT is returned as the starting port number
get_random_port () {
	local plist
	local cont
	local collisions
	local i
	local MAXCOLLISION=1000
	cont=1
	collisions=0
	while test "$cont" = 1; do
		#netstat -n -A ip -A ip6 -a | sed -e "s/^.*:\([0-9]*\) .*$/\1/"
		RND_PORT=$(( $RANDOM + 5354 ))
		# depending on uname try to check for collisions in port numbers
		case "`uname`" in
		linux|Linux)
			plist=`netstat -n -A ip -A ip6 -a 2>/dev/null | sed -e 's/^.*:\([0-9]*\) .*$/\1/'`
		;;
		FreeBSD|freebsd|NetBSD|netbsd|OpenBSD|openbsd)
			plist=`netstat -n -a | grep "^[ut][dc]p[46] " | sed -e 's/^.*\.\([0-9]*\) .*$/\1/'`
		;;
		Solaris|SunOS)
			plist=`netstat -n -a | sed -e 's/^.*\.\([0-9]*\) .*$/\1/' | grep '^[0-9]*$'`
		;;
		*)
			plist=""
		;;
		esac
		cont=0
		for (( i=0 ; i < $1 ; i++ )); do
			if echo "$plist" | grep '^'`expr $i + $RND_PORT`'$' >/dev/null 2>&1; then
				cont=1;
				collisions=`expr $collisions + 1`
			fi
		done
		if test $collisions = $MAXCOLLISION; then
			error "too many collisions getting random port number"
		fi
	done
}

# wait for server to go up, pass <logfilename> <string to watch>
# $1 : logfilename
# $2 : string to watch for.
# exits with failure if it does not come up
wait_server_up () {
	local MAX_UP_TRY=120
	local WAIT_THRES=30
	local try
	for (( try=0 ; try <= $MAX_UP_TRY ; try++ )) ; do
		if test -f $1 && fgrep "$2" $1 >/dev/null; then
			#echo "done on try $try"
			break;
		fi
		if test $try -eq $MAX_UP_TRY; then
			echo "Server in $1 did not go up!"
			cat $1
			exit 1;
		fi
		if test $try -ge $WAIT_THRES; then
			sleep 1
		fi
	done
}

# wait for ldns-testns to come up
# $1 : logfilename that is watched.
wait_ldns_testns_up () {
	wait_server_up "$1" "Listening on port"
}

# wait for unbound to come up
# string 'Start of service' in log.
# $1 : logfilename that is watched.
wait_unbound_up () {
	wait_server_up "$1" "start of service"
}

# wait for petal to come up
# string 'petal start' in log.
# $1 : logfilename that is watched.
wait_petal_up () {
	wait_server_up "$1" "petal start"
}

# wait for nsd to come up
# string nsd start in log.
# $1 : logfilename that is watched.
wait_nsd_up () {
	wait_server_up "$1" " started (NSD "
}

# wait for server to go up, pass <logfilename> <string to watch> <badstr>
# $1 : logfile
# $2 : success string
# $3 : failure string
wait_server_up_or_fail () {
	local MAX_UP_TRY=120
	local WAIT_THRES=30
	local try
	for (( try=0 ; try <= $MAX_UP_TRY ; try++ )) ; do
		if test -f $1 && fgrep "$2" $1 >/dev/null; then
			echo "done on try $try"
			break;
		fi
		if test -f $1 && fgrep "$3" $1 >/dev/null; then
			echo "failed on try $try"
			break;
		fi
		if test $try -eq $MAX_UP_TRY; then
			echo "Server in $1 did not go up!"
			cat $1
			exit 1;
		fi
		if test $try -ge $WAIT_THRES; then
			sleep 1
		fi
	done
}

# kill a pid, make sure and wait for it to go down.
# $1 : pid to kill
kill_pid () {
	local MAX_DOWN_TRY=120
	local WAIT_THRES=30
	local try
	kill $1
	for (( try=0 ; try <= $MAX_DOWN_TRY ; try++ )) ; do
		if kill -0 $1 >/dev/null 2>&1; then
			:
		else
			#echo "done on try $try"
			break;
		fi
		if test $try -eq $MAX_DOWN_TRY; then
			echo "Server in $1 did not go down! Send SIGKILL"
			kill -9 $1 >/dev/null 2>&1
		fi
		if test $try -ge $WAIT_THRES; then
			sleep 1
		fi
		# re-send the signal
		kill $1 >/dev/null 2>&1
	done
	return 0
}

# set doxygen path, so that make doc can find doxygen
set_doxygen_path () {
	if test -x '/home/wouter/bin/doxygen'; then
	        export PATH="/home/wouter/bin:$PATH"
	fi
}

# Print the current test step in the output
teststep () {
	echo
	echo "STEP [ $1 ]"
}
