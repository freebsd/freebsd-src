#!/bin/sh
#
# (C)opyright 2012 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
test_init() {
	mkdir -p results
	find_touch
	set_core $name 1
}

set_core() {
	if [ -n "${FINDLEAKS}" -a -x /bin/mdb ] ; then
		_findleaks=1
	else
		_findleaks=0
	fi
	if [ -x /bin/coreadm ] ; then
		_cn="$1.$2.core"
		coreadm -p "${PWD}/$_cn"
	else
		_cn=
	fi
}

test_end_leak() {
	if [ $1 -ne 0 ] ; then
		if [ ${_findleaks} = 1 -a -f $_cn ] ; then
			echo "==== ${name}:${n} ====" >> leaktest
			echo '::findleaks' | mdb ../i86/ipftest $_cn >> leaktest
			rm $_cn
		else
			exit 2;
		fi
	fi
}

check_results() {
	cmp expected/$1 results/$1
	status=$?
	if [ $status = 0 ] ; then
		$TOUCH $1
	fi
}

find_touch() {
	if [ -f /bin/touch ] ; then
		TOUCH=/bin/touch
	else
		if [ -f /usr/bin/touch ] ; then
			TOUCH=/usr/bin/touch
		else
			if [ -f /usr/ucb/touch ] ; then
				TOUCH=/usr/ucb/touch
			fi
		fi
	fi
}
