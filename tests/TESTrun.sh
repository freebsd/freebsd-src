#!/bin/sh

TZ=GMT0; export TZ
srcdir=${SRCDIR-..}

# make it absolute for later use.
pwd
srcdir=`cd $srcdir && pwd`

# this should be run from the compiled build directory,
# with srcdir= set to wherever the source code is.
# not from the tests directory.
echo RUNNING from ${srcdir}

passedfile=`pwd`/tests/.passed
failedfile=`pwd`/tests/.failed
failureoutput=`pwd`/tests/failure-outputs.txt
mkdir -p tests/NEW
mkdir -p tests/DIFF
cat /dev/null > ${failureoutput}

runComplexTests()
{
  for i in ${srcdir}/tests/*.sh
  do
    case $i in
        ${srcdir}/tests/TEST*.sh) continue;;
        ${srcdir}/tests/\*.sh) continue;;
    esac
    : echo Running $i
    (sh $i ${srcdir})
  done
  passed=`cat ${passedfile}`
  failed=`cat ${failedfile}`
}

runSimpleTests()
{
  only=$1
  cat ${srcdir}/tests/TESTLIST | while read name input output options
  do
    case $name in
      \#*) continue;;
      '') continue;;
    esac
    rm -f core
    [ "$only" != "" -a "$name" != "$only" ] && continue
    SRCDIR=${srcdir}
    export SRCDIR
    # I hate shells with their stupid, useless subshells.
    passed=`cat ${passedfile}`
    failed=`cat ${failedfile}`
    (
    if ${srcdir}/tests/TESTonce $name ${srcdir}/tests/$input ${srcdir}/tests/$output "$options" 
    then
      passed=`expr $passed + 1`
      echo $passed >${passedfile}
    else
      failed=`expr $failed + 1`
      echo $failed >${failedfile}
    fi
    if [ -d tests/COREFILES ]; then
        if [ -f core ]; then mv core tests/COREFILES/$name.core; fi
    fi)
    [ "$only" != "" -a "$name" = "$only" ] && break
  done
  # I hate shells with their stupid, useless subshells.
  passed=`cat ${passedfile}`
  failed=`cat ${failedfile}`
}

passed=0
failed=0
echo $passed >${passedfile}
echo $failed >${failedfile}
if [ $# -eq 0 ]
then
  runComplexTests
  runSimpleTests
elif [ $# -eq 1 ]
then
  runSimpleTests $1
else
  echo "Usage: $0 [test_name]"
  exit 30
fi

# exit with number of failing tests.
echo '------------------------------------------------'
printf "%4u tests failed\n" $failed
printf "%4u tests passed\n" $passed
echo
if [ -z "$SKIPPASSED" ]; then
    cat ${failureoutput}
fi
echo
echo
exit $failed
