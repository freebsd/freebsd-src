#!/bin/sh
# $FreeBSD$

cd `dirname $0`
cmd="./`basename $0 .t`"

make ${cmd} >/dev/null 2>&1

IFS=
n=0

run()
{
	result=`${cmd} -t $2 $3 $4 2>&1`
	if [ $? -eq 0 ]; then
		echo -n "ok $1"
	else
		echo -n "not ok $1"
	fi
	echo " -" $5
	echo ${result} | grep -E "SERVER|CLIENT" | while read line; do
		echo "# ${line}"
	done
}

echo "1..15"

for desc in \
	"Sending, receiving cmsgcred" \
	"Receiving sockcred (listening socket has LOCAL_CREDS) # TODO" \
	"Receiving sockcred (accepted socket has LOCAL_CREDS) # TODO" \
	"Sending cmsgcred, receiving sockcred # TODO" \
	"Sending, receiving timestamp"
do
	n=`expr ${n} + 1`
	run ${n} stream "" ${n} "STREAM ${desc}"
done

i=0
for desc in \
	"Sending, receiving cmsgcred" \
	"Receiving sockcred # TODO" \
	"Sending cmsgcred, receiving sockcred # TODO" \
	"Sending, receiving timestamp"
do
	i=`expr ${i} + 1`
	n=`expr ${n} + 1`
	run ${n} dgram "" ${i} "DGRAM ${desc}"
done

run 10 stream -z 1 "STREAM Sending, receiving cmsgcred (no control data)"
run 11 stream -z 4 "STREAM Sending cmsgcred, receiving sockcred (no control data) # TODO"
run 12 stream -z 5 "STREAM Sending, receiving timestamp (no control data)"

run 13 dgram -z 1 "DGRAM Sending, receiving cmsgcred (no control data)"
run 14 dgram -z 3 "DGRAM Sending cmsgcred, receiving sockcred (no control data) # TODO"
run 15 dgram -z 4 "DGRAM Sending, receiving timestamp (no control data)"
