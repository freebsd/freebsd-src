#! /bin/sh
# test that the empty file means no pattern
# and an empty pattern means match all.

: ${srcdir=.}

failures=0

# should return 0 found a match
echo "abcd" | ${GREP} -E -e '' > /dev/null 2>&1
if test $? -ne 0 ; then
        echo "Status: Wrong status code, test \#1 failed"
        failures=1
fi

# should return 1 found no match
echo "abcd" | ${GREP} -E -f /dev/null  > /dev/null 2>&1
if test $? -ne 1 ; then
        echo "Status: Wrong status code, test \#2 failed"
        failures=1
fi

# should return 0 found a match
echo "abcd" | ${GREP} -E -f /dev/null -e "abc" > /dev/null 2>&1
if test $? -ne 0 ; then
        echo "Status: Wrong status code, test \#3 failed"
        failures=1
fi

exit $failures
