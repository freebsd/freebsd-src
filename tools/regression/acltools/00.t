#!/bin/sh
#
# This is a wrapper script to run tools-posix.test.
#
# If any of the tests fails, here is how to debug it: go to
# the directory with problematic filesystem mounted on it,
# and do /path/to/test run /path/to/test tools-posix.test, e.g.
#
# /usr/src/tools/regression/acltools/run /usr/src/tools/regression/acltools/tools-posix.test
#
# Output should be obvious.
#
# $FreeBSD$
#

echo "1..4"

if [ `whoami` != "root" ]; then
	echo "not ok 1 - you need to be root to run this test."
	exit 1
fi

TESTDIR=`dirname $0`

# Set up the test filesystem.
MD=`mdconfig -at swap -s 10m`
MNT=`mktemp -dt acltools`
newfs /dev/$MD > /dev/null
mount -o acls /dev/$MD $MNT
if [ $? -ne 0 ]; then
	echo "not ok 1 - mount failed."
	exit 1
fi

echo "ok 1"

cd $MNT

# First, check whether we can crash the kernel by creating too many
# entries.  For some reason this won't work in the test file.
touch xxx
i=0;
while :; do i=$(($i+1)); setfacl -m u:$i:rwx xxx 2> /dev/null; if [ $? -ne 0 ]; then break; fi; done
chmod 600 xxx
rm xxx
echo "ok 2"

perl $TESTDIR/run $TESTDIR/tools-posix.test > /dev/null

if [ $? -eq 0 ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

cd /
umount -f $MNT
rmdir $MNT
mdconfig -du $MD

echo "ok 4"

