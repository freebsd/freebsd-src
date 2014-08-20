#!/bin/bash 
#
# This is a simple script to convert the a XML testcae to C source file, compile it, run it, and report the result.
# Usage:
# This script is designed to run under the libdom/test directory.
# 
# domts="testcases" dtd="dom1-interfaces.xml" level="level1" ./run-single-test.sh
#
# The above command will convert the XML testcase in directory testcases/tests/level/core and 
# use dom1-interfaces.xml to convert it. 
# This script will generate a output/ directory in libdom/test, and in that directory, there is a same structure
# as in DOMTS XML testcases files.
#
#  This file is part of libdom test suite.
#  Licensed under the MIT License,
#				 http://www.opensource.org/licenses/mit-license.php
#  Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>

level=${level:-"level1"};
module=${module:-"core"};
domts=${domts:?"The \$domts must be assigned some value"};
output=${output:-"output"};
dtd=${dtd:?"The DTD file must be given"};

testdir="$domts"/tests/"$level"/"$module"
log="$output"/"$level"/"$module"/test.log;

src="testutils/comparators.c testutils/domtsasserts.c testutils/foreach.c testutils/list.c testutils/load.c testutils/utils.c testutils/domtscondition.c"
domdir="../build-Linux-Linux-debug-lib-static"
ldflags="-L$domdir -ldom -L/usr/local/lib  -lwapcaplet -L/usr/lib -lxml2 -lhubbub -lparserutils"
#ldflags="-L/usr/lib -lm -lz -L$domdir -ldom -L/usr/local/lib  -lwapcaplet -lxml2 -lhubbub -lparserutils"
cflags="-Itestutils/ -I../bindings/xml  -I../include -I../bindings/hubbub -I/usr/local/include"

total=0;
fail=0;
pass=0;
conversion=0;
compile=0;
run=0;
nsupport=0;

# Create the directories if necessary
if [ ! -e "$ouput" ]; then
	mkdir -p "$output";
fi
if [ ! -e "$level" ]; then
	mkdir -p "$output"/"$level";
fi
if [ ! -e "$module" ]; then
	mkdir -p "$output"/"$level"/"$module";
fi

# Prepare the test files
if [ -e "files" ]; then
	rm -fr files;
fi
cp -fr "$testdir/files" ./;

while read test; do
	total=$(($total+1));

	file=`basename "$test"`;
	name=${file%%.xml};
	
	cfile="$output"/"$level"/"$module"/"$name".c;
	ofile="$output"/"$level"/"$module"/"$name";
	tfile="$testdir"/"$file";

	echo -n "$file:";

	# Generate the test C program
	out=`perl transform.pl "$dtd" "$tfile" 2>&1  >"${cfile}.unindent"`;
	if [ "$?" != "0" ]; then
		fail=$((fail+1));
		conversion=$((conversion+1));
		echo "$tfile Conversion Error:" >& 3;
		echo "Please make sure you have XML::XPath perl module installed!" >& 3;
		echo "$out" >&3;
		echo -e "----------------------------------------\n\n" >& 3;
		echo "failed!";
		rm -fr "${cfile}.unindent";
		continue;
	fi
	out=`indent "${cfile}.unindent" -o "$cfile" 2>&1`;
	if [ "$?" != "0" ]; then
		rm -fr "${cfile}.unindent";
		fail=$((fail+1));
		conversion=$((conversion+1));
		echo "$tfile Conversion Error:" >& 3;
		echo "$out" >& 3;
		echo -e "----------------------------------------\n\n" >& 3;
		echo "failed!";
		continue;
	fi
	rm -fr "${cfile}.unindent";

	# Compile it now
	out=` ( gcc -g $cflags $src $cfile $ldflags -o "$ofile" ) 2>&1`;
	if [ "$?" != "0" ]; then
		fail=$((fail+1));
		compile=$((compile+1));
		echo "$tfile Compile Error:" >& 3;
		echo "$out" >& 3;
		echo -e "----------------------------------------\n\n" >& 3;
		echo "failed!";
		continue;
	fi
	
	# Run the test and test the result
	cd files;
	out=$(../$ofile 2>&1);
	ret="$?";
	if [ "$ret" != "0" ]; then
		cd ..;
		fail=$((fail+1));
		if [ "$ret" == "9" ]; then
			nsupport=$((nsupport+1))
			echo "$tfile Not Support:" >& 3;
			echo "$out" >& 3;
			echo -e "----------------------------------------\n\n" >& 3;
			echo "not supported!";
		else
			run=$((run+1));
			echo "$tfile Run Error:" >& 3;
			echo "$out" >& 3;
			echo -e "----------------------------------------\n\n" >& 3;
			echo "failed!";
		fi
		continue;
	fi
	cd ..;

	pass=$((pass+1));
	echo "passed.";

done 3>&1 < <(echo $1);

echo "Total:  $total";
echo "Passed: $pass";
echo "Failed: $fail";
echo "Conversion Error: $conversion";
echo "Compile Error:	$compile";
echo "Run Error:	$run";
echo "Not Support:	$nsupport";
