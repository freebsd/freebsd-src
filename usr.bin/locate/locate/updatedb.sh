#!/bin/sh
#
# (c) Wolfram Schneider, Berlin. September 1995. Public domain.
#
# updatedb - update locate database for local mounted filesystems
#
# $Id: updatedb.sh,v 1.3 1996/04/20 21:55:21 wosch Exp wosch $

LOCATE_CONFIG="/etc/locate.rc"
if [ -f "$LOCATE_CONFIG" -a -r "$LOCATE_CONFIG" ]; then
       . $LOCATE_CONFIG
fi

# The directory containing locate subprograms
: ${LIBEXECDIR=/usr/libexec}; export LIBEXECDIR

PATH=$LIBEXECDIR:/bin:/usr/bin:$PATH; export PATH


: ${mklocatedb=locate.mklocatedb}	 # make locate database program
: ${FCODES=/var/db/locate.database}	 # the database
: ${SEARCHPATHS="/"}		# directories to be put in the database
: ${PRUNEPATHS="/tmp /usr/tmp /var/tmp"} # unwanted directories
: ${FILESYSTEMS="ufs"}			 # allowed filesystems 
: ${find=find}

case X"$SEARCHPATHS" in 
	X) echo "$0: empty variable SEARCHPATHS"; exit 1;; esac
case X"$FILESYSTEMS" in 
	X) echo "$0: empty variable FILESYSTEMS"; exit 1;; esac

# Make a list a paths to exclude in the locate run
excludes="! (" or=""
for fstype in $FILESYSTEMS
do
       excludes="$excludes $or -fstype $fstype"
       or="-or"
done
excludes="$excludes ) -prune"

case X"$PRUNEPATHS" in
	X) ;;
	*) for path in $PRUNEPATHS
           do 
		excludes="$excludes -or -path $path -prune"
	   done;;
esac

tmp=${TMPDIR=/tmp}/_updatedb$$
trap 'rm -f $tmp' 0 1 2 3 5 10 15
		
# search locally
# echo $find $SEARCHPATHS $excludes -or -print && exit
if $find $SEARCHPATHS $excludes -or -print 2>/dev/null | 
        $mklocatedb > $tmp
then
	case X"`$find $tmp -size -257c -print`" in
		X) cat $tmp > $FCODES;;
		*) echo "updatedb: locate database $tmp is empty"
		   exit 1
	esac
fi
