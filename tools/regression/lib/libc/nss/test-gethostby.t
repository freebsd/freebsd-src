#!/bin/sh
# $FreeBSD$

do_test() {
	number=$1
	comment=$2
	opt=$3
	if ./$executable $opt; then
		echo "ok $number - $comment"
	else
		echo "not ok $number - $comment"
	fi
}

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

echo 1..32
#Tests for gethostby***() functions
#IPv4-driven testing
do_test 1 'gethostbyname2() (IPv4)' '-4 -n -f mach'
do_test 2 'gethostbyaddr() (IPv4)' '-4 -a -f mach'
do_test 3 'gethostbyname2()-getaddrinfo() (IPv4)' '-4 -2 -f mach'
do_test 4 'gethostbyaddr()-getnameinfo() (IPv4)' '-4 -i -f mach'
do_test 5 'gethostbyname2() snapshot (IPv4)'\
	'-4 -n -s snapshot_htname4 -f mach'
do_test 6 'gethostbyaddr() snapshot (IPv4)'\
	'-4 -a -s snapshot_htaddr4 -f mach'

#IPv6-driven testing
do_test 7 'gethostbyname2() (IPv6)' '-6 -n -f mach'
do_test 8 'gethostbyaddr() (IPv6)' '-6 -a -f mach'
do_test 9 'gethostbyname2()-getaddrinfo() (IPv6)' '-6 -2 -f mach'
do_test 10 'gethostbyaddr()-getnameinfo() (IPv6)' '-6 -i -f mach'
do_test 11 'gethostbyname2() snapshot (IPv6)'\
	'-6 -n -s snapshot_htname6 -f mach'
do_test 12 'gethostbyaddr() snapshot (IPv6)'\
	'-6 -a -s snapshot_htaddr6 -f mach'
	
#Mapped IPv6-driven testing (getaddrinfo() equality test is useless here)
do_test 13 'gethostbyname2() (IPv6 mapped)' '-m -n -f mach'
do_test 14 'gethostbyaddr() (IPv6 mapped)' '-m -a -f mach'
do_test 15 'gethostbyname2() snapshot (IPv6 mapped)'\
	'-m -n -s snapshot_htname6map -f mach'
do_test 16 'gethostbyaddr() snapshot (IPv6 mapped)'\
	'-m -a -s snapshot_htaddr6map -f mach'

#Tests for getipnodeby***() functions
#IPv4-driven testing
do_test 17 'getipnodebyname() (IPv4)' '-o -4 -n -f mach'
do_test 18 'getipnodebyaddr() (IPv4)' '-o -4 -a -f mach'
do_test 19 'getipnodebyname()-getaddrinfo() (IPv4)' '-o -4 -2 -f mach'
do_test 20 'getipnodebyaddr()-getnameinfo() (IPv4)' '-o -4 -i -f mach'
do_test 21 'getipnodebyname() snapshot (IPv4)'\
	'-o -4 -n -s snapshot_ipnodename4 -f mach'
do_test 22 'getipnodebyname() snapshot (IPv4)'\
	'-o -4 -a -s snapshot_ipnodeaddr4 -f mach'

#IPv6-driven testing
do_test 23 'getipnodebyname() (IPv6)' '-o -6 -n -f mach'
do_test 24 'getipnodebyaddr() (IPv6)' '-o -6 -a -f mach'
do_test 25 'getipnodebyname()-getaddrinfo() (IPv6)' '-o -6 -2 -f mach'
do_test 26 'getipnodebyaddr()-getnameinfo() (IPv6)' '-o -6 -i -f mach'
do_test 27 'getipnodebyname() snapshot (IPv6)'\
	'-o -6 -n -s snapshot_ipnodename6 -f mach'
do_test 28 'getipnodebyaddr() snapshot (IPv6)'\
	'-o -6 -a -s snapshot_ipnodeaddr6 -f mach'

#Mapped IPv6-driven testing (getaddrinfo() equality test is useless here)
do_test 29 'getipnodebyname() (IPv6)' '-o -m -n -f mach'
do_test 30 'getipnodebyaddr() (IPv6)' '-o -m -a -f mach'
do_test 31 'getipnodebyname() snapshot (IPv6)'\
	'-o -m -n -s snapshot_ipnodename6map -f mach'
do_test 32 'getipnodebyaddr() snapshot (IPv6)'\
	'-o -m -a -s snapshot_ipnodeaddr6map -f mach'
