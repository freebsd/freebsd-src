#!/bin/sh
#---------------------------------------------------------------------------
#
#	isdn telephone answering
#	-------------------------
#
#	$Id: isdntel.sh,v 1.7 1998/12/18 17:17:57 hm Exp $
#
#	last edit-date: [Fri Dec 18 18:05:26 1998]
#
#	-hm	answering script
#	-hm	curses interface
#	-hm	update for release
#
#---------------------------------------------------------------------------
LIBDIR=/usr/local/lib/isdn
VARDIR=/var/isdn
DEVICE=/dev/i4btel0

# sounds 
MESSAGE=$LIBDIR/msg.g711a
BEEP=$LIBDIR/beep.g711a

# dd options
SKIP=25

# max message size
MAXMSIZ=100

# src and dst telephone numbers
src=
dst=

# current date
DATE=`date`

# check if directory exists
if [ ! -d $VARDIR ]
then
	mkdir $VARDIR
fi

# get options
set -- `/usr/bin/getopt D:d:s: $*`

if [ $? != 0 ]
then
	echo "usage2: play -D device -d <dest-telno> -s <src-telno>"
	exit 1
fi

# process options
for i
do
	case $i in
		-D)
			DEVICE=$2; shift; shift;
			;;
		-d)
			dst=$2; shift; shift;
			;;
		-s)
			src=$2; shift; shift;
			;;
		--)
			shift; break;
			;;
	esac
done

# this is a __MUST__ in order to use the fullscreen inteface !!!

FILEDATE=`date \+%y%m%d%H%M%S`

# echo message to phone
if [ -f $MESSAGE ]
then
	/bin/dd of=$DEVICE if=$MESSAGE bs=2k >/dev/null 2>&1
fi

# echo beep to phone
if [ -f $BEEP ]
then
	/bin/dd of=$DEVICE if=$BEEP bs=2k >/dev/null 2>&1
fi

# start time
START=`date \+%s`

# get message from caller
/bin/dd if=$DEVICE of=$VARDIR/$FILEDATE-$dst-$src skip=$SKIP bs=2k count=$MAXMSIZ >/dev/null 2>&1

# end time
END=`date \+%s`

# duration
TIME=`expr $END - $START`

# save recorded message
if [ -f $VARDIR/$FILEDATE-$dst-$src ]
then
	mv $VARDIR/$FILEDATE-$dst-$src $VARDIR/$FILEDATE-$dst-$src-$TIME
fi

exit 0
