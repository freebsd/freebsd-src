#! /bin/sh
#
# $Id: rcs-to-cvs.sh,v 1.2 1995/07/15 03:40:34 jimb Exp $
# Based on the CVS 1.0 checkin csh script.
# Contributed by Per Cederqvist <ceder@signum.se>.
# Rewritten in sh by David MacKenzie <djm@cygnus.com>.
#
#   Copyright (c) 1989, Brian Berliner
#
#   You may distribute under the terms of the GNU General Public License.
#
#############################################################################
#
# Check in sources that previously were under RCS or no source control system.
#
# The repository is the directory where the sources should be deposited.
#
# Traverses the current directory, ensuring that an
# identical directory structure exists in the repository directory.  It
# then checks the files in in the following manner:
#
#		1) If the file doesn't yet exist, check it in as revision 1.1
#
# The script also is somewhat verbose in letting the user know what is
# going on.  It prints a diagnostic when it creates a new file, or updates
# a file that has been modified on the trunk.
#
# Bugs: doesn't put the files in branch 1.1.1
#       doesn't put in release and vendor tags
#
#############################################################################

usage="Usage: rcs-to-cvs [-v] [-m message] [-f message_file] repository"
vbose=0
message=""
message_file=/usr/tmp/checkin.$$
got_one=0

if [ $# -lt 1 ]; then
    echo "$usage" >&2
    exit 1
fi

while [ $# -ne 0 ]; do
    case "$1" in
        -v)
            vbose=1
	    ;;
	-m)
	    shift
	    echo $1 > $message_file
	    got_one=1
	    ;;
	-f)
	    shift
	    message_file=$1
	    got_one=2
	    ;;
	*)
	    break
    esac
    shift
done

if [ $# -lt 1 ]; then
    echo "$usage" >&2
    exit 1
fi

repository=$1
shift

if [ -z "$CVSROOT" ]; then
    echo "Please the environmental variable CVSROOT to the root" >&2
    echo "	of the tree you wish to update" >&2
    exit 1
fi

if [ $got_one -eq 0 ]; then
    echo "Please Edit this file to contain the RCS log information" >$message_file
    echo "to be associated with this directory (please remove these lines)">>$message_file
    ${EDITOR-/usr/ucb/vi} $message_file
    got_one=1
fi

# Ya gotta share.
umask 0

update_dir=${CVSROOT}/${repository}
[ ! -d ${update_dir} ] && mkdir $update_dir

if [ -d SCCS ]; then
    echo SCCS files detected! >&2
    exit 1
fi
if [ -d RCS ]; then
    co RCS/*
fi

for name in * .[a-zA-Z0-9]*
do
    case "$name" in
    RCS | *~ | \* | .\[a-zA-Z0-9\]\* ) continue ;;
    esac
    echo $name
    if [ $vbose -ne 0 ]; then 
	echo "Updating ${repository}/${name}"
    fi
    if [ -d "$name" ]; then
	if [ ! -d "${update_dir}/${name}" ]; then
	    echo "WARNING: Creating new directory ${repository}/${name}"
	    mkdir "${update_dir}/${name}"
	    if [ $? -ne 0 ]; then
		echo "ERROR: mkdir failed - aborting" >&2
		exit 1
	    fi
	fi
	cd "$name"
	if [ $? -ne 0 ]; then
	    echo "ERROR: Couldn\'t cd to $name - aborting" >&2
	    exit 1
	fi
	if [ $vbose -ne 0 ]; then
	    $0 -v -f $message_file "${repository}/${name}"
	else
	    $0 -f $message_file "${repository}/${name}"
	fi
	if [ $? -ne 0 ]; then 
	    exit 1
	fi
	cd ..
    else	# if not directory 
	if [ ! -f "$name" ]; then
	    echo "WARNING: $name is neither a regular file"
	    echo "	   nor a directory - ignored"
	    continue
	fi
	file="${update_dir}/${name},v"
	comment=""
	if grep -s '\$Log.*\$' "${name}"; then # If $Log keyword
	    myext=`echo $name | sed 's,.*\.,,'`
	    [ "$myext" = "$name" ] && myext=
	    case "$myext" in
		c | csh | e | f | h | l | mac | me | mm | ms | p | r | red | s | sh | sl | cl | ml | el | tex | y | ye | yr | "" )
		;;

		* )
		echo "For file ${file}:"
		grep '\$Log.*\$' "${name}"
		echo -n "Please insert a comment leader for file ${name} > "
		read comment
		;;
	    esac
	fi
	if [ ! -f "$file" ]; then	# If not exists in repository
	    if [ ! -f "${update_dir}/Attic/${name},v" ]; then
	        echo "WARNING: Creating new file ${repository}/${name}"
		if [ -f RCS/"${name}",v ]; then
			echo "MSG: Copying old rcs file."
			cp RCS/"${name}",v "$file"
		else
   		    if [ -n "${comment}" ]; then
		        rcs -q -i -c"${comment}" -t${message_file} -m'.' "$file"
		    fi
	            ci -q -u1.1 -t${message_file} -m'.' "$file" 
	            if [ $? -ne 0 ]; then
		        echo "ERROR: Initial check-in of $file failed - aborting" >&2
		        exit 1
	            fi
		fi
	    else 
		file="${update_dir}/Attic/${name},v"
		echo "WARNING: IGNORED: ${repository}/Attic/${name}"
		continue
	    fi
	else	# File existed 
	    echo "ERROR: File exists in repository: Ignored: $file"
	    continue
	fi
    fi
done

[ $got_one -eq 1 ] && rm -f $message_file

exit 0
