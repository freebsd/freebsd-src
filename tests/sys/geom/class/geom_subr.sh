#!/bin/sh
# $FreeBSD$

devwait()
{
	while :; do
		if [ -c /dev/${class}/${name} ]; then
			return
		fi
		sleep 0.2
	done
}

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

	if [ -f "$TEST_MDS_FILE" ]; then
		while read test_md; do
			# The "#" tells the TAP parser this is a comment
			echo "# Removing test memory disk: $test_md"
			mdconfig -d -u $test_md
		done < $TEST_MDS_FILE
	fi
	rm -f "$TEST_MDS_FILE"
}

if [ $(id -u) -ne 0 ]; then
	echo '1..0 # SKIP tests must be run as root'
	exit 0
fi
# If the geom class isn't already loaded, try loading it.
if ! kldstat -q -m g_${class}; then
	if ! geom ${class} load; then
		echo "1..0 # SKIP could not load module for geom class=${class}"
		exit 0
	fi
fi

# Need to keep track of the test md devices to avoid the scenario where a test
# failing will cause the other tests to bomb out, or a test failing will leave
# a large number of md(4) devices lingering around
: ${TMPDIR=/tmp}
export TMPDIR
if ! TEST_MDS_FILE=$(mktemp ${TMPDIR}/test_mds.XXXXXX); then
	echo 'Failed to create temporary file for tracking the test md(4) devices'
	echo 'Bail out!'
	exit 1
fi
