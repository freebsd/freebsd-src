#!/bin/sh
# $FreeBSD$

if [ $(id -u) -ne 0 ]; then
	echo 'Tests must be run as root'
	echo 'Bail out!'
	exit 1
fi
kldstat -q -m g_${class} || geom ${class} load || exit 1

devwait()
{
	while :; do
		if [ -c /dev/${class}/${name} ]; then
			return
		fi
		sleep 0.2
	done
}

# Need to keep track of the test md devices to avoid the scenario where a test
# failing will cause the other tests to bomb out, or a test failing will leave
# a large number of md(4) devices lingering around
: ${TMPDIR=/tmp}
export TMPDIR
TEST_MDS_FILE=${TMPDIR}/test_mds

attach_md()
{
	local test_md

	test_md=$(mdconfig -a "$@") || exit
	echo $test_md >> $TEST_MDS_FILE || exit
	echo $test_md
}

geom_test_cleanup()
{
	local test_md

	if [ -f $TEST_MDS_FILE ]; then
		while read test_md; do
			# The "#" tells the TAP parser this is a comment
			echo "# Removing test memory disk: $test_md"
			mdconfig -d -u $test_md
		done < $TEST_MDS_FILE
	fi
}
