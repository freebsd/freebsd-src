#!/bin/sh -
#
# $Id$
#
# Run nightly periodic scripts
#
# usage: periodic { daily | weekly | monthly } - run standard periodic scripts
#        periodic /absolute/path/to/directory  - run periodic scripts in dir
#

if [ $# -lt 1 ] ; then
    echo "usage: $0 <directory of files to execute>" 1>&2
    exit 1
fi

dir=$1
run=`basename $dir`

# If a full path was not specified, assume default cron area

if [ "$dir" = "$run" ] ; then
    dir="/etc/cron.d/$dir"
fi

if [ ! -d $dir ] ; then
    ( echo "$0: $dir not found"
      echo ""
      echo "usage: $0 <directory of files to execute>"
    ) 1>&2
    exit 1
fi

# Check and see if there is work to be done, if not, exit silently
# this is not an error condition.

if [ "`basename $dir/*`" = "*" ] ; then
    exit 0
fi

host=`hostname -s`
echo "Subject: $host $run run output"

# Execute each executable file in the directory.  If the x bit is not
# set, assume the user didn't really want us to muck with it (it's a
# README file or has been disabled).

for file in $dir/* ; do
    if [ -x $file ] ; then
	$file
    fi
done
