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

host=`hostname`
export host
tmp_output=`mktemp ${TMPDIR:-/tmp}/periodic.XXXXXXXXXX`

# Execute each executable file in the directory list.  If the x bit is not
# set, assume the user didn't really want us to muck with it (it's a
# README file or has been disabled).

for arg
do
    # Where's our output going ?
    eval output=\$${arg##*/}_output
    case "$output" in
    /*) pipe="cat >>$output";;
    "") pipe=cat;;
    *)  pipe="mail -s '$host ${arg##*/} run output' $output";;
    esac

    success=YES info=YES badconfig=NO	# Defaults when ${run}_* aren't YES/NO
    for var in success info badconfig
    do
        case $(eval echo "\$${arg##*/}_show_$var") in
        [Yy][Ee][Ss]) eval $var=YES;;
        [Nn][Oo])     eval $var=NO;;
        esac
    done

    case $arg in
    /*) if [ -d "$arg" ]
        then
            dirlist="$arg"
        else
            echo "$0: $arg not found" >&2 
            continue
        fi;;
    *)  dirlist=
        for top in /etc/periodic ${local_periodic}
        do
            [ -d $top/$arg ] && dirlist="$dirlist $top/$arg"
        done;;
    esac

    {
        empty=TRUE
        processed=0
        for dir in $dirlist
        do
            for file in $dir/*
            do
                if [ -x $file -a ! -d $file ]
                then
                    output=TRUE
                    processed=$(($processed + 1))
                    $file </dev/null >$tmp_output 2>&1
                    rc=$?
                    if [ -s $tmp_output ]
                    then
                      case $rc in
                      0)  [ $success = NO ] && output=FALSE;;
                      1)  [ $info = NO ] && output=FALSE;;
                      2)  [ $badconfig = NO ] && output=FALSE;;
                      esac
                      [ $output = TRUE ] && { cat $tmp_output; empty=FALSE; }
                    fi
                    cp /dev/null $tmp_output
                fi
            done
            rm -f $tmp_output
        done
        if [ $empty = TRUE ]
        then
          [ $processed = 1 ] && plural= || plural=s
          echo "No output from the $processed file$plural processed"
        fi
    } | eval $pipe
done
