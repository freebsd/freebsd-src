#!/bin/sh

mkdir -p NEW
mkdir -p DIFF
passed=0
failed=0

# first run any specific tests.
for i in *.sh
do
  case $i in TEST*.sh) continue;; esac

  if sh ./$i >DIFF/$i.result
  then
      echo $i: passed.
      rm -f DIFF/$i.result
      passed=$(($passed + 1))
  else
      echo $i: failed.
      failed=$(($failed + 1))
  fi          
done 

echo $passed >.passed
echo $failed >.failed

# now run typical tests
cat TESTLIST | while read name input output options
do
  case $name in
      \#*) continue;;
      '') continue;;
  esac

  if ./TESTonce $name $input $output "$options"
  then
      echo $name: passed.
      rm -f DIFF/$output.diff
      passed=$(($passed + 1))
      echo $passed >.passed
  else
      echo $name: failed.
      failed=$(($failed + 1))
      echo $failed >.failed
  fi
done 

# I hate shells with their stupid, useless subshells.
passed=`cat .passed`
failed=`cat .failed`

# exit with number of failing tests.
echo 
echo
printf "%4u tests failed\n" $failed
printf "%4u tests passed\n" $passed
echo
echo
exit $failed      




