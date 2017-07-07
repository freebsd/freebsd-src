#!/bin/sh

dir=$1; shift
while [ $# -gt 0 ]; do
	file=$1
	cmp -s $file $dir/$file
	if [ $? != 0 ]; then
		echo "+ rm $dir/$file"
		rm -f $dir/$file
		echo "+ cp $file $dir/$file"
		cp $file $dir/$file
	fi
	shift
done
