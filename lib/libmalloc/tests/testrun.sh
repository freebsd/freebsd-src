#! /bin/sh
time=time
awk 'BEGIN {
	maxtime = 15000;
	maxsize = 610; isize = 50;
	maxlife = 8010; ilife = 100;
	hdrfmt = "echo \"Maxtime = %d, Maxsize = %d, Maxlife = %d\"\n";
	fmt = "$time $cmd -t %d -s %d -l %d\n";
    }
    END {
	for (i = 10; i < maxsize; i += isize) {
		for (j = 10; j < maxlife; j += ilife) {
			printf hdrfmt, maxtime, i, j;
			printf fmt, maxtime, i, j;
			printf fmt, maxtime, i, j;
			printf fmt, maxtime, i, j;
		}
	}
    }' /dev/null > /tmp/runs.$$
for i
do
	ext=`expr "$i" : "simumalloc.exe\(.*\)"`
	date
	echo $i
	cmd="./$i"
	. /tmp/runs.$$ > times$ext 2>&1
	date
done
