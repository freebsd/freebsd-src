#!/bin/sh
# (c) Wolfram Schneider, Berlin. June 1996. Public domain.
#
# TEST.sh - check if test(1) or builtin test works
#
# $Id: $

# force a specified test program, e.g. `env test=/bin/test sh TEST.sh'
: ${test=test}		

ERROR=0 FAILED=0

t ()
{
	# $1 -> exit code
	# $2 -> $test expression

	echo -n "$test $2 "

	# check for syntax errors
	syntax="`eval $test $2 2>&1`"
	if test -z "$syntax"; then

	case $1 in
		0) if eval $test $2; then echo " OK"; else failed;fi;;
		1) if eval $test $2; then failed; else echo " OK";fi;;
	esac

	else
		error
	fi
}

error () 
{
	echo ""; echo "	$syntax"
	ERROR=`expr $ERROR + 1`
}

failed () 
{
	echo ""; echo "	failed"
	FAILED=`expr $FAILED + 1`
}


t 0 'b = b' 
t 1 'b != b' 
t 0 '\( b = b \)' 
t 1 '! \( b = b \)' 
t 1 '! -f /etc/passwd'

t 0 '-h = -h'
t 0 '-o = -o'

t 1 '-f = h'
t 1 '-h = f'
t 1 '-o = f'
t 1 'f = -o'
t 0 '\( -h = -h \)'
t 1 '\( a = -h \)'
t 1 '\( -f = h \)'


t 1 '\( -f = h \)'

t 0 '-h = -h -o a'
t 0 '\( -h = -h \) -o 1'

t 0 '-h = -h -o -h = -h'
t 0 '\( -h = -h \) -o \( -h = -h \)'

t 0 '-d /'
t 0 '-d / -a a != b'
t 1 '-z "-z"'
t 0 '-n -n'
t 0 '0 -eq 0'
t 0 '\( 0 -eq 0 \)'
t 1 '1 -eq 0 -o a = a -a 1 -eq 0 -o a = aa'

t 0 '0'
t 0 '\( 0 \)'
t 0 '-E'
t 0 '-X -a -X'
t 0 '-XXX'
t 0 '\( -E \)'
t 0 'true -o X'
t 0 'true -o -X'
t 0 '\( \( \( a = a \) -o 1 \) -a 1 \) -a true'
t 1 '-h /'
t 0 '-r /'
t 1 '-w /'
t 0 '-x /bin/sh'
t 0 '-c /dev/null'
t 0 '-b /dev/fd0a -o -b /dev/rfd0a -o true'
t 0 '-f /etc/passwd'
t 0 '-s /etc/passwd'
t 1 '! \( 700 -le 1000 -a -n "1" -a "20" = "20" \)'
t 0 '100 -eq 100'
t 0 '100 -lt 200'
t 1 '1000 -lt 200'
t 0 '1000 -gt 200'
t 0 '1000 -ge 200'
t 0 '1000 -ge 1000'
t 1 '2 -ne 2'

echo ""
echo "Syntax errors: $ERROR Failed: $FAILED"

