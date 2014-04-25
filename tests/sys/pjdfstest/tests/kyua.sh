. "$dir/../misc.sh"

if [ -d "$PJDFSTEST_TARGET" ]
then
	cd "$PJDFSTEST_TARGET"
else
	echo "1..1"
	echo "not ok - you must first define your target to test against via \$PJDFSTEST_TARGET"
	exit 0
fi
