#!/bin/sh

set -e

path="$(dirname $0)"

test="$1"
makedir="$2"
if [ -z $test ] ; then
	echo "This script helps bisect test failures, given a test case."
	echo ""
	echo "Use it like this:"
	echo "  git bisect start"
	echo "  git bisect bad <commit>"
	echo "  git bisect good <commit>"
	echo "  git bisect run $0 <test name> [<compile directory>]"
	echo ""
	echo "(the compile directory is optional, use it if you want to"
	echo "use an out-of-tree kernel build."
	echo ""
	echo "Note that, of course, you have to have a working vm-run setup."
	exit 200 # exit git bisect run if called that way
fi

if [ -n "$makedir" ] ; then
	cd "$makedir"
fi

yes '' | make oldconfig || exit 125
make -j8 || exit 125

output=$(mktemp)
if [ $? -ne 0 ] ; then
        exit 202
fi
finish() {
        rm -f $output
}
trap finish EXIT

"$path/vm-run.sh" $test 2>&1 | tee $output

grep -q 'ALL-PASSED' $output && exit 0 || exit 1
