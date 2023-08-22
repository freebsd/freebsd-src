#!/bin/sh

: ${ARG_MAX:=524288}
base=$(dirname $(realpath "$0"))

echo "1..2"

waitfor() {
	flagfile=$1

	iter=0

	while [ ! -f ${flagfile} ] && [ ${iter} -lt 50 ]; do
		sleep 0.10
		iter=$((iter + 1))
	done

	if [ ! -f ${flagfile} ]; then
		return 1
	fi
}

sentinel="findme=test-$$"
sentinelsz=$(printf "${sentinel}" | wc -c | tr -d '[[:space:]]')
name="pgrep -f"
spin="${base}/spin_helper"
flagfile="pgrep_f_short.flag"

${spin} --short ${flagfile} ${sentinel} &
chpid=$!
if ! waitfor ${flagfile}; then
	echo "not ok - $name"
else
	pid=$(pgrep -f ${sentinel})
	if [ "$pid" = "$chpid" ]; then
		echo "ok - $name"
	else
		echo "not ok - $name"
	fi
fi
kill $chpid

name="pgrep -f long args"
flagfile="pgrep_f_long.flag"
${spin} --long ${flagfile} ${sentinel} &
chpid=$!
if ! waitfor ${flagfile}; then
	echo "not ok - $name"
else
	pid=$(pgrep -f ${sentinel})
	if [ "$pid" = "$chpid" ]; then
		echo "ok - $name"
	else
		echo "not ok - $name"
	fi
fi
kill $chpid
