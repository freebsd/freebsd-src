#! /tmp/gawk -f 
	{ ccount += length($0) }
END { printf "average line length is %2.4f\n", ccount/NR}
