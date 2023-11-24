#!/bin/sh

# Bug 214923 - kqueue hangs with busy loop
# Test scenario by: Jihyun Yu <yjh0502@gmail.com>

# https://people.freebsd.org/~pho/stress/log/watchman.txt
# Fixed by: r310302

. ../default.cfg

[ -z `which watchman` ] && { echo "watchman is not installed"; exit 0; }

daemon sh -c "(cd ../testcases/swap; ./swap -t 5m -i 20 -h -l 100)" > \
    /dev/null

dir=/tmp/watchman
rm -rf $dir
mkdir -p $dir
cd $dir

mkdir -p foo bar
seq -w 0 100 | xargs -n1 -I{} touch foo/{}.c

echo '["subscribe","./","mysub",{"fields":["name"],"expression":["allof",'\
    '["type","f"],["not","empty"],["suffix","c"]]}]' | \
    watchman -p -j --server-encoding=json > /dev/null &
pids=$!
while true; do find bar/ -type f | xargs -n1 -P5 -I'{}' mv '{}' foo; done &
pids="$pids $!"
while true; do find foo/ -type f | xargs -n1 -P5 -I'{}' mv '{}' bar; done &
pids="$pids $!"

sleep 180
while pgrep -q swap; do
	pkill -9 swap
done
kill -9 $pids
pkill watchman
wait
cd /
sleep 1
rm -rf $dir
exit 0
