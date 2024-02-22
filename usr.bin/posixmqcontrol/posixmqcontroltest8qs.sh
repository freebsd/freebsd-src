#!/bin/sh
# testing create, info, and send operations applied to multiple queue names at once.
# recv accepts a single queue name so draining is done one queue at a time.
subject='posixmqcontrol'
prefix='/posixmqcontroltest'

list=
for i in 1 2 3 4 5 6 7 8
do
  topic="${prefix}${i}"
  ${subject} info -q "${topic}" 2>/dev/null
  if [ $? == 0 ]; then
    echo "sorry, $topic exists."
    exit 1
  fi
  list="${list} -q ${topic}"
done

${subject} create -d 2 -s 64 ${list}
if [ $? != 0 ]; then
  exit 1
fi

ignore=$( ${subject} info ${list} )
if [ $? != 0 ]; then
  exit 1
fi

${subject} send -c 'this message sent to all listed queues.' ${list}
if [ $? != 0 ]; then
  exit 1
fi

# we can only drain one message at a time.
for i in 1 2 3 4 5 6 7 8
do
  topic="${prefix}${i}"
  ignore=$( ${subject} recv -q "${topic}" )
  if [ $? != 0 ]; then
    exit 1
  fi
done

${subject} rm ${list}
if [ $? == 0 ]; then
  echo "Pass!"
  exit 0
fi

exit 1
