#!/bin/sh -
#
# $Id: periodic.sh,v 1.4 1997/08/16 17:08:35 pst Exp $
#
# Run nightly periodic scripts
#
# usage: periodic { daily | weekly | monthly } - run standard periodic scripts
#        periodic /absolute/path/to/directory  - run periodic scripts in dir
#

usage () {
    echo "usage: $0 <directory of files to execute>" 1>&2
    echo "or     $0 { daily | weekly | monthly }"    1>&2
    exit 1
}

if [ $# -lt 1 ] ; then
    usage
fi

# If possible, check /etc/rc.conf to see if there are additional dirs to check
if [ -r /etc/rc.conf ] ; then
    . /etc/rc.conf
fi

dir=$1
run=`basename $dir`

# If a full path was not specified, check the standard cron areas

if [ "$dir" = "$run" ] ; then
    dirlist=""
    for top in /etc/periodic ${local_periodic} ; do
	if [ -d $top/$dir ] ; then
	    dirlist="${dirlist} $top/$dir"
	fi
    done

# User wants us to run stuff in a particular directory
else
   for dir in $* ; do
       if [ ! -d $dir ] ; then
	   echo "$0: $dir not found" 1>&2
	   exit 1
       fi
   done

   dirlist="$*"
fi

host=`hostname -s`
echo "Subject: $host $run run output"

# Execute each executable file in the directory list.  If the x bit is not
# set, assume the user didn't really want us to muck with it (it's a
# README file or has been disabled).

for dir in $dirlist ; do
    for file in $dir/* ; do
	if [ -x $file ] ; then
	    $file
	fi
    done
done
