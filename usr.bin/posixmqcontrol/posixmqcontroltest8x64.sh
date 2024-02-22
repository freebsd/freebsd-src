#!/bin/sh
# exercises create, info, send and recv subcommands.

subject='posixmqcontrol'
topic='/test123'

${subject} info -q "$topic" 2>/dev/null
if [ $? == 0 ]; then
  echo "sorry, $topic exists."
  exit 1
fi

# create trivial queue that can hold 8 messages of 64 bytes each.
${subject} create -q "$topic" -s 64 -d 8
if [ $? != 0 ]; then
  exit 1
fi

info=$(${subject} info -q "$topic")
if [ $? != 0 ]; then
  exit 1
fi
expected='MSGSIZE: 64'
actual=$(echo "${info}" | grep 'MSGSIZE: ')
if [ "$expected" != "$actual" ]; then
  echo "EXPECTED: $expected"
  echo "  ACTUAL: $actual"
  exit 1
fi
expected='MAXMSG: 8'
actual=$(echo "${info}" | grep 'MAXMSG: ')
if [ "$expected" != "$actual" ]; then
  echo "EXPECTED: $expected"
  echo "  ACTUAL: $actual"
  exit 1
fi
expected='CURMSG: 0'
actual=$(echo "${info}" | grep 'CURMSG: ')
if [ "$expected" != "$actual" ]; then
  echo "EXPECTED: $expected"
  echo "  ACTUAL: $actual"
  exit 1
fi

# write eight messages of increasing priority.
for i in 1 2 3 4 5 6 7 8
do
  ${subject} send -q "$topic" -c "message $i" -p "$i"
  if [ $? != 0 ]; then
    exit 1
  fi
done

info=$(${subject} info -q "$topic")
if [ $? != 0 ]; then
  exit
fi
expected='CURMSG: 8'
actual=$(echo "${info}" | grep 'CURMSG: ')
if [ "$expected" != "$actual" ]; then
  echo "EXPECTED: $expected"
  echo "  ACTUAL: $actual"
  exit 1
fi

# expect the eight messages to appear in priority order.
for i in 8 7 6 5 4 3 2 1
do
  expected='['"$i"']: message '"$i"
  actual=$(${subject} recv -q "$topic")
  if [ $? != 0 ]; then
    exit
  fi
  if [ "$expected" != "$actual" ]; then
    echo "EXPECTED: $expected"
    echo "  ACTUAL: $actual"
    exit 1
  fi
done

info=$(${subject} info -q "$topic")
if [ $? != 0 ]; then
  exit 1
fi
expected='CURMSG: 0'
actual=$(echo "${info}" | grep 'CURMSG: ')
if [ "$expected" != "$actual" ]; then
  echo "EXPECTED: $expected"
  echo "  ACTUAL: $actual"
  exit 1
fi

${subject} rm -q "$topic"
if [ $? == 0 ]; then
  echo "Pass!"
  exit 0
fi

exit 1
