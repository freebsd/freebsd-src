#! /bin/sh
# takes output of testrun and massages into columnar form for easier
# evaluation and graphing
cat $* | tr ',' ' ' |
    awk 'BEGIN {
	    printf " Maxtime Maxsize Maxlife  Sbrked Alloced Wastage";
	    printf "    Real    User     Sys\n";
	}
	$1 == "Maxtime" {
	    if (t != 0) {
		printf "%8d%8d%8d%8d%8d %7.2f", t, s, l, sb, ma, w;
		printf " %7.1f %7.1f %7.1f\n", mr, mu, ms;
	    }
	    t = $3;
	    s = $6;
	    l = $9;
	    mr = 100000;
	    mu = 100000;
	    ms = 100000;
	    next;
         }
	 $1 == "Sbrked" {
	    sb = $2;
	    ma = $4;
	    w = $6;
	    next;
	 }
	 $2 == "real" {
	    if ($1 < mr) mr = $1;
	    if ($3 < mu) mu = $3;
	    if ($5 < ms) ms = $5;
	    next;
	 }
	 END {
	    if (t != 0) {
		printf "%8d%8d%8d%8d%8d %7.2f", t, s, l, sb, ma, w;
		printf " %7.1f %7.1f %7.1f\n", mr, mu, ms;
	    }
	 }
    '
