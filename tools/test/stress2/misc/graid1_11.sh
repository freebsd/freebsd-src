#!/bin/sh

# g_mirror: don't fail reads while losing next-to-last disk

# Fixed by https://cgit.FreeBSD.org/src/commit/?id=5d5f44623eb3d121d528060d131ee5d6bcd63489
# Test scenario by: Andriy Gapon <avg@FreeBSD.org>

set -u

cleanup()
{
	echo "cleanup" 2>&1
	gnop destroy -f ${dev1}.nop
	gnop destroy -f ${dev2}.nop
	gmirror destroy testmirror
	mdconfig -d -u ${dev2#md}
	mdconfig -d -u ${dev1#md}
}

list_descendants()
{
	local children

	children=$(pgrep -P "$1")
	for pid in $children ; do
		list_descendants "$pid"
	done
	echo "$children"
}

# Note that the size of gnop providers is smaller than the
# size of backing md-s to avoid gmirror auto-tasting.
for i in 1 2; do
	eval dev$i=$(mdconfig -a -t swap -s 1024m)
	eval gnop create -d 8 -s 1000m \${dev$i}
done

trap cleanup EXIT INT TERM QUIT

gmirror load 2>/dev/null || true
gmirror label -b round-robin -F testmirror ${dev1}.nop ${dev2}.nop

(
	#my_pid=$(exec sh -c 'echo "$PPID"')

	sleep 100000 &
	sentry=$!

	children=""
	for i in $(seq 8) ; do
		(
		while dd if=/dev/mirror/testmirror of=/dev/null > /dev/null ; ds=$? ; [ $ds -eq 0 ] ; do
			:
		done
		if [ $ds -lt 128 ] ; then
			# Not killed
			echo "dd exited with $ds" 1>&2
			kill $sentry 2>/dev/null
		fi
		) &

		children="${children:+${children},}$!"

		sleep 0.1
	done

	wait $sentry
	pkill -P ${children} -x dd

	# Reap background children
	wait
) &
runner=$!

# Give dd-s some time to get running
sleep 2

# Destroy one of the members
echo "destroying one member" 1>&2
gnop destroy -f ${dev1}.nop

count=0
while kill -0 $runner 2>/dev/null && [ $count -lt 5 ] ; do
	sleep 1
	count=$((count + 1))
done

if ! kill -0 $runner 2>/dev/null ; then
	echo "the test has self-terminated" 1>&2
	ret=1
else
	echo "the test is stuck, killing..." 1>&2
	ret=0
	kill $(list_descendants $runner) 2>/dev/null
fi

# Reap background processes
wait

# Just in case
sleep 5

exit $ret
