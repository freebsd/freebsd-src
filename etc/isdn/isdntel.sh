#!/bin/sh
#---------------------------------------------------------------------------
#
#	isdn telephone answering
#	-------------------------
#
# $FreeBSD: src/etc/isdn/isdntel.sh,v 1.4 1999/09/13 15:44:20 sheldonh Exp $
#
#	last edit-date: [Thu May 20 11:45:04 1999]
#
#---------------------------------------------------------------------------
#FreeBSD < 3.1, NetBSD, OpenBSD, BSD/OS
#LIBDIR=/usr/local/lib/isdn
#FreeBSD 3.1 and up
LIBDIR=/usr/share/isdn

VARDIR=/var/isdn
DEVICE=/dev/i4btel0

# sounds
MESSAGE=${LIBDIR}/msg.al
BEEP=${LIBDIR}/beep.al

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
if [ ! -d "${VARDIR}" ]
then
	mkdir ${VARDIR}
fi

# get options
if ! set -- `/usr/bin/getopt D:d:s: $*`; then
	echo "usage2: play -D device -d <dest-telno> -s <src-telno>"
	exit 1
fi

# process options
for i ; do
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
if [ -r "${MESSAGE}" ]; then
	/bin/dd of=${DEVICE} if=${MESSAGE} bs=2k >/dev/null 2>&1
fi

# echo beep to phone
if [ -r "${BEEP}" ]; then
	/bin/dd of=${DEVICE} if=${BEEP} bs=2k >/dev/null 2>&1
fi

# start time
START=`date \+%s`

# get message from caller
/bin/dd if=${DEVICE} of=${VARDIR}/${FILEDATE}-${dst}-${src} skip=${SKIP} bs=2k count=${MAXMSIZ} >/dev/null 2>&1

# end time
END=`date \+%s`

# duration
TIME=`expr ${END} - ${START}`

# save recorded message
if [ -r "${VARDIR}/${FILEDATE}-${dst}-${src}" ]; then
	mv ${VARDIR}/${FILEDATE}-${dst}-${src} ${VARDIR}/${FILEDATE}-${dst}-${src}-${TIME}
fi

exit 0
