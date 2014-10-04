#!/bin/sh
# $FreeBSD$

jail_name_to_jid()
{
	local check_name="$1"
	(
		line="$(jls -n 2> /dev/null | grep  name=$check_name  )"
		for nv in $line; do
			local name="${nv%=*}"
			if [ "${name}" = "jid" ]; then
				eval $nv
				echo $jid
				break
			fi
		done
	)
}

base=pkill_j_test

echo "1..3"

name="pkill -j <jid>"
if [ `id -u` -eq 0 ]; then
	sleep=$(pwd)/sleep.txt
	ln -sf /bin/sleep $sleep
        jail -c path=/ name=${base}_1_1 ip4.addr=127.0.0.1 \
            command=daemon -p ${PWD}/${base}_1_1.pid $sleep 5 &

        jail -c path=/ name=${base}_1_2 ip4.addr=127.0.0.1 \
            command=daemon -p ${PWD}/${base}_1_2.pid $sleep 5 &

	$sleep 5 &
	sleep 0.5
	jid1=$(jail_name_to_jid ${base}_1_1)
	jid2=$(jail_name_to_jid ${base}_1_2)
	jid="${jid1},${jid2}"
	if pkill -f -j "$jid" $sleep && sleep 0.5 &&
	    ! -f ${PWD}/${base}_1_1.pid &&
	    ! -f ${PWD}/${base}_1_2.pid ; then
		echo "ok 1 - $name"
	else
		echo "not ok 1 - $name"
	fi 2>/dev/null
	rm -f $sleep
	[ -f ${PWD}/${base}_1_1.pid ] && kill $(cat ${PWD}/${base}_1_1.pid)
	[ -f ${PWD}/${base}_1_2.pid ] && kill $(cat ${PWD}/${base}_1_2.pid)
	wait
else
	echo "ok 1 - $name # skip Test needs uid 0."
fi

name="pkill -j any"
if [ `id -u` -eq 0 ]; then
	sleep=$(pwd)/sleep.txt
	ln -sf /bin/sleep $sleep
        jail -c path=/ name=${base}_2_1 ip4.addr=127.0.0.1 \
            command=daemon -p ${PWD}/${base}_2_1.pid $sleep 5 &

        jail -c path=/ name=${base}_2_2 ip4.addr=127.0.0.1 \
            command=daemon -p ${PWD}/${base}_2_2.pid $sleep 5 &

	$sleep 5 &
	sleep 0.5
	chpid3=$!
	if pkill -f -j any $sleep && sleep 0.5 &&
	    [ ! -f ${PWD}/${base}_2_1.pid -a
	      ! -f ${PWD}/${base}_2_2.pid ] && kill $chpid3; then
		echo "ok 2 - $name"
	else
		echo "not ok 2 - $name"
	fi 2>/dev/null
	rm -f $sleep
	[ -f ${PWD}/${base}_2_1.pid ] && kill $(cat ${PWD}/${base}_2_1.pid)
	[ -f ${PWD}/${base}_2_2.pid ] && kill $(cat ${PWD}/${base}_2_2.pid)
	wait
else
	echo "ok 2 - $name # skip Test needs uid 0."
fi

name="pkill -j none"
if [ `id -u` -eq 0 ]; then
	sleep=$(pwd)/sleep.txt
	ln -sf /bin/sleep $sleep
	daemon -p ${PWD}/${base}_3_1.pid $sleep 5
	jail -c path=/ name=${base}_3_2 ip4.addr=127.0.0.1 \
            command=daemon -p ${PWD}/${base}_3_2.pid $sleep 5 &
	sleep 1
	if pkill -f -j none "$sleep 5" && sleep 1 &&
	    [ ! -f ${PWD}/${base}_3_1.pid -a -f ${PWD}/${base}_3_2.pid ] ; then
		echo "ok 3 - $name"
	else
		ls ${PWD}/*.pid
		echo "not ok 3 - $name"
	fi 2>/dev/null
	rm -f $sleep
	[ -f ${PWD}/${base}_3_1.pid ] && kill $(cat ${base}_3_1.pid)
	[ -f ${PWD}/${base}_3_2.pid ] && kill $(cat ${base}_3_2.pid)
else
	echo "ok 3 - $name # skip Test needs uid 0."
fi
