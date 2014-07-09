#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

name="chown -f root:wheel file"
if [ `id -u` -eq 0 ]; then
	echo "ok 3 - $name # skip Test must not be uid 0."
else
    touch file
    output=$(chown -f root:wheel file 2>&1)
    if [ $? -eq 0 -a -z "$output" ]
    then
        echo "ok 1 - $name"
    else
        echo "not ok 1 - $name"
    fi
    rm file
fi
