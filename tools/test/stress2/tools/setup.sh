#!/bin/sh

# Create the local configuration file.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

allconfig=${allconfig:-/tmp/stress2.d/`hostname`}
[ -f $allconfig ] && grep -q "^testuser" $allconfig && exit 0
set -e
echo -n "Enter non-root test user name: "
read testuser
id $testuser > /dev/null 2>&1 ||
    { echo "User \"$testuser\" not found."; exit 1; }
[ $testuser ] || exit 1
su $testuser -c 'echo Hello' 2>&1 | grep -qE '^Hello$' ||
    { echo "User \"$testuser\" not usable."; exit 1; }
mkdir -p `dirname $allconfig`
(
	echo '# Local stress2 config file'
	echo '# Overwrite default values in stress2/default.cfg'
	echo
	echo '# nfs_export=${nfs_export:-t1:/tmp}'
	echo '# BLASTHOST=${BLASTHOST:-t1}'
	echo '# RUNDIR=${RUNDIR:-/work/stressX}'
	echo '# diskimage=${diskimage:-/work/diskfile}'
 	echo
	echo "testuser=$testuser"
) >> $allconfig
cat > `dirname $allconfig`/README <<EOF
The /tmp/stress2.d directory contain the following files:

bench.sh.log	Performance info for the bench.sh test
elapsed		The runtime for each test
excessive	Test that ran for more than 30 minutes
all.exclude	Local list of tests to exclude, one test per line starting in column one
fail		List of the failed test
last		Last test run
list		Current list of tests to run
log		List of test run
output		Output from the last test run
`hostname`	Local configuration file
vmstat		Memory leak report
EOF
[ -f README ] && echo && cat README
exit 0
