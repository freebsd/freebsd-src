#!/bin/bash
#
# This is a simple script used to test libdom memory leakage. 
# Usage: 
# You should firstly run "run-test.sh", which will genrate a test output directory. In that
# directory, there are C source files and corresponding executables.
# Go to the test output directory. For example , for core, level 1, it is output/level1/core
# And run this script as ../../../leak-test.sh "log-file"
#
#  This file is part of libdom test suite.
#  Licensed under the MIT License,
#                 http://www.opensource.org/licenses/mit-license.php
#  Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>

log=$1;
totla=0;
leak=0;
ok=0;
while read f; do
	#Test defnitely lost
	echo -n "$f: " >&3;
	echo -n "$f: ";
	dl=$(valgrind "$f" 2>&1 | grep "definitely lost" | sed -e 's/definitely lost//g' -e 's/bytes in//g' -e 's/blocks.//g' -e 's/^.*://g'  -e 's/ //g' -e 's/,//g');
	pl=$(valgrind "$f" 2>&1 | grep "possibly lost" | sed -e 's/possibly lost//g' -e 's/bytes in//g' -e 's/blocks.//g' -e 's/^.*://g'  -e 's/ //g' -e 's/,//g');

	total=$((total+1));
	if [ "$dl" -eq "00" -a "$pl" -eq "00" ]; then
		echo "ok..."  >&3;
		echo "ok...";
		ok=$((ok+1));
	else
		echo "leaked!" >&3;
		echo "leaked!";
		leak=$((leak+1));
	fi

done 3>"$log" < <(find ./ -perm -o=x  -type f -print);

echo "Total: $total" >>"$log";
echo "Leak:  $leak" >>"$log";
echo "Ok:	 $ok" >>"$log";

echo "Total: $total";
echo "Leak:  $leak";
echo "Ok:	 $ok";
