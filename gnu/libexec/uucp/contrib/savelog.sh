#! /bin/sh
# @(#)util/savelog.sh	1.4 26 Oct 1991 22:49:39
#
# savelog - save a log file
#
#    Copyright (C) 1987, 1988 Ronald S. Karr and Landon Curt Noll
# 
# See the file COPYING, distributed with smail, for restriction
# and warranty information.
#
# usage: savelog [-m mode] [-u user] [-g group] [-t] [-c cycle] [-l] file...
#
#	-m mode	  - chmod log files to mode
#	-u user	  - chown log files to user
#	-g group  - chgrp log files to group
#	-c cycle  - save cycle versions of the logfile	(default: 7)
#	-t	  - touch file
#	-l	  - don't compress any log files	(default: compress)
#	file 	  - log file names
#
# The savelog command saves and optionally compresses old copies of files
# into an 'dir'/OLD sub-directory.  The 'dir' directory is determined from
# the directory of each 'file'.  
#
# Older version of 'file' are named:
#
#		OLD/'file'.<number><compress_suffix>
#
# where <number> is the version number, 0 being the newest.  By default,
# version numbers > 0 are compressed (unless -l prevents it). The
# version number 0 is never compressed on the off chance that a process
# still has 'file' opened for I/O.
#
# If the 'file' does not exist or if it is zero length, no further processing
# is performed.  However if -t was also given, it will be created.
#
# For files that do exist and have lengths greater than zero, the following 
# actions are performed.
#
#	1) Version numered files are cycled.  That is version 6 is moved to
#	   version 7, version is moved to becomes version 6, ... and finally
#	   version 0 is moved to version 1.  Both compressed names and
#	   uncompressed names are cycled, regardless of -t.  Missing version 
#	   files are ignored.
#
#	2) The new OLD/file.1 is compressed and is changed subject to 
#	   the -m, -u and -g flags.  This step is skipped if the -t flag 
#	   was given.
#
#	3) The main file is moved to OLD/file.0.
#
#	4) If the -m, -u, -g or -t flags are given, then file is created 
#	   (as an empty file) subject to the given flags.
#
#	5) The new OLD/file.0 is chanegd subject to the -m, -u and -g flags.
#
# Note: If the OLD sub-directory does not exist, it will be created 
#       with mode 0755.
#
# Note: If no -m, -u or -g flag is given, then the primary log file is 
#	not created.
#
# Note: Since the version numbers start with 0, version number <cycle>
#       is never formed.  The <cycle> count must be at least 2.
#
# Bugs: If a process is still writing to the file.0 and savelog
#	moved it to file.1 and compresses it, data could be lost.
#	Smail does not have this problem in general because it
#	restats files often.

# common location
PATH="X_UTIL_PATH_X:X_SECURE_PATH_X"; export PATH
COMPRESS="X_COMPRESS_X"
COMP_FLAG="X_COMP_FLAG_X"
DOT_Z="X_DOT_Z_X"
CHOWN="X_CHOWN_X"
GETOPT="X_UTIL_BIN_DIR_X/getopt"

# parse args
exitcode=0	# no problems to far
prog=$0
mode=
user=
group=
touch=
count=7
set -- `$GETOPT m:u:g:c:lt $*`
if [ $# -eq 0 -o  $? -ne 0 ]; then
	echo "usage: $prog [-m mode][-u user][-g group][-t][-c cycle][-l] file ..." 1>&2
	exit 1
fi
for i in $*; do
	case $i in
	-m) mode=$2; shift 2;;
	-u) user=$2; shift 2;;
	-g) group=$2; shift 2;;
	-c) count=$2; shift 2;;
	-t) touch=1; shift;;
	-l) COMPRESS=""; shift;;
	--) shift; break;;
	esac
done
if [ "$count" -lt 2 ]; then
	echo "$prog: count must be at least 2" 1>&2
	exit 2
fi

# cycle thru filenames
while [ $# -gt 0 ]; do

	# get the filename
	filename=$1
	shift

	# catch bogus files
	if [ -b "$filename" -o -c "$filename" -o -d "$filename" ]; then
		echo "$prog: $filename is not a regular file" 1>&2
		exitcode=3
		continue
	fi

	# if not a file or empty, do nothing major
	if [ ! -s $filename ]; then
		# if -t was given and it does not exist, create it
		if [ ! -z "$touch" -a ! -f $filename ]; then 
			touch $filename
			if [ "$?" -ne 0 ]; then
				echo "$prog: could not touch $filename" 1>&2
				exitcode=4
				continue
			fi
			if [ ! -z "$user" ]; then 
				$CHOWN $user $filename
			fi
			if [ ! -z "$group" ]; then 
				chgrp $group $filename
			fi
			if [ ! -z "$mode" ]; then 
				chmod $mode $filename
			fi
		fi
		continue
	fi

	# be sure that the savedir exists and is writable
	savedir=`expr "$filename" : '\(.*\)/'`
	if [ -z "$savedir" ]; then
		savedir=./OLD
	else
		savedir=$savedir/OLD
	fi
	if [ ! -s $savedir ]; then
		mkdir $savedir
		if [ "$?" -ne 0 ]; then
			echo "$prog: could not mkdir $savedir" 1>&2
			exitcode=5
			continue
		fi
		chmod 0755 $savedir
	fi
	if [ ! -d $savedir ]; then
		echo "$prog: $savedir is not a directory" 1>&2
		exitcode=6
		continue
	fi
	if [ ! -w $savedir ]; then
		echo "$prog: directory $savedir is not writable" 1>&2
		exitcode=7
		continue
	fi

	# deterine our uncompressed file names
	newname=`expr "$filename" : '.*/\(.*\)'`
	if [ -z "$newname" ]; then
		newname=$savedir/$filename
	else
		newname=$savedir/$newname
	fi

	# cycle the old compressed log files
	cycle=`expr $count - 1`
	rm -f $newname.$cycle $newname.$cycle$DOT_Z
	while [ "$cycle" -gt 1 ]; do
		# --cycle
		oldcycle=$cycle
		cycle=`expr $cycle - 1`
		# cycle log
		if [ -f $newname.$cycle$DOT_Z ]; then
			mv -f $newname.$cycle$DOT_Z $newname.$oldcycle$DOT_Z
		fi
		if [ -f $newname.$cycle ]; then
			# file was not compressed for some reason move it anyway
			mv -f $newname.$cycle $newname.$oldcycle
		fi
	done

	# compress the old uncompressed log if needed
	if [ -f $newname.0 ]; then
		if [ -z "$COMPRESS" ]; then
			newfile=$newname.1
			mv $newname.0 $newfile
		else
			newfile=$newname.1$DOT_Z
			$COMPRESS $COMP_FLAG < $newname.0 > $newfile
			rm -f $newname.0
		fi
		if [ ! -z "$user" ]; then 
			$CHOWN $user $newfile
		fi
		if [ ! -z "$group" ]; then 
			chgrp $group $newfile
		fi
		if [ ! -z "$mode" ]; then 
			chmod $mode $newfile
		fi
	fi

	# move the file into the file.0 holding place
	mv -f $filename $newname.0

	# replace file if needed
	if [ ! -z "$touch" -o ! -z "$user" -o \
	     ! -z "$group" -o ! -z "$mode" ]; then 
		touch $filename
	fi
	if [ ! -z "$user" ]; then 
		$CHOWN $user $filename
	fi
	if [ ! -z "$group" ]; then 
		chgrp $group $filename
	fi
	if [ ! -z "$mode" ]; then 
		chmod $mode $filename
	fi

	# fix the permissions on the holding place file.0 file
	if [ ! -z "$user" ]; then 
		$CHOWN $user $newname.0
	fi
	if [ ! -z "$group" ]; then 
		chgrp $group $newname.0
	fi
	if [ ! -z "$mode" ]; then 
		chmod $mode $newname.0
	fi
done
exit $exitcode
