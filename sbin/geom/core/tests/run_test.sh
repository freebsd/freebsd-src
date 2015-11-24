#!/bin/sh
# $FreeBSD$

MD=34
: ${TMPDIR=/tmp}
TMP=$TMPDIR/$$

# The testcases seem to have been broken when the geom_sim provider was removed
echo "# these tests need to be rewritten; they were broken in r113434"
echo "1..0 # SKIP"
exit 0

set -e

# Start from the right directory so we can find all our data files.
testsdir=$(realpath $(dirname $0))

# Print the test header
set -- $testsdir/Data/disk.*.xml
echo "1..$#"
data_files="$@"

trap "rm -f $TMP; mdconfig -d -u $MD" EXIT INT TERM

set +e

refdir=$(realpath $(mktemp -d Ref.XXXXXX))
for f in $data_files; do
	b=`basename $f`
	mdconfig -d -u $MD

	i=0
	while [ $i -lt 2 -a -c /dev/md$MD ]; do
		sleep 1
		: $(( i += 1 ))
	done
	if [ -c /dev/md$MD ] ; then
		echo "Bail out!"
		echo "/dev/md$MD is busy"
		exit 1
	fi
	if ! $testsdir/MdLoad/MdLoad md${MD} $f; then
		echo "not ok - $b # MdLoad failed"
		continue
	fi
	if [ ! -f $refdir/$b ]; then
		if [ -f $testsdir/Ref/$b ] ; then
			grep -v '\$FreeBSD.*\$' $testsdir/Ref/$b > $refdir/$b
		else
			diskinfo /dev/md${MD}* > $refdir/$b
			continue
		fi
	fi
	diskinfo /dev/md${MD}* | diff -u $refdir/$b - > $TMP
	if [ $? -eq 0 ]; then
		echo "ok - $b"
	else
		echo "not ok - $b"
		sed 's/^/# /' $TMP
	fi
done
