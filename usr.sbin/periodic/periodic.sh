#!/bin/sh -
#
# $FreeBSD$
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

# If possible, check the global system configuration file, 
# to see if there are additional dirs to check
if [ -r /etc/defaults/periodic.conf ]; then
    . /etc/defaults/periodic.conf
    source_periodic_confs
fi

dirlist=

# If a full path was not specified, check the standard cron areas

for dir
do
    case "$dir" in
    /*)
	if [ -d "$dir" ]
	then
	    dirlist="$dirlist $dir"
	else
	    echo "$0: $dir not found" >&2 
	fi;;
    *)
	for top in /etc/periodic ${local_periodic}
	do
	    [ -d $top/$dir ] && dirlist="$dirlist $top/$dir"
	done;;
    esac
done

host=`hostname`
export host
tmp_output=/var/run/periodic.$$

# Execute each executable file in the directory list.  If the x bit is not
# set, assume the user didn't really want us to muck with it (it's a
# README file or has been disabled).

for dir in $dirlist
do
    eval output=\$${dir##*/}_output
    case "$output" in
    /*) pipe="cat >>$output";;
    *)  pipe="mail -s '$host ${dir##*/} run output' ${output:-root}";;
    esac

    success=YES info=YES badconfig=NO	# Defaults when ${run}_* aren't YES/NO
    for var in success info badconfig
    do
	case $(eval echo "\$${dir##*/}_show_$var") in
	[Yy][Ee][Ss]) eval $var=YES;;
	[Nn][Oo])     eval $var=NO;;
	esac
    done

    for file in $dir/*
    do
	if [ -x $file -a ! -d $file ]
	then
	    $file </dev/null >$tmp_output 2>&1
	    case $? in
	    0)  [ $success = YES ] && cat $tmp_output;;
	    1)  [ $info = YES ] && cat $tmp_output;;
	    2)  [ $badconfig = YES ] && cat $tmp_output;;
	    *)  cat $tmp_output;;
	    esac
	    rm -f $tmp_output
	fi
    done | eval $pipe
done
