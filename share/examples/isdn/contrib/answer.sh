#!/bin/sh
#---------------------------------------------------------------------------
#
#	answer script for i4b isdnd
#	---------------------------
#
#	last edit-date: [Fri May 25 15:21:05 2001]
#
# $FreeBSD$
#
#---------------------------------------------------------------------------
VARDIR=/var/isdn
LIBDIR=/usr/local/lib/isdn
LOGFILE=/tmp/answer.log

NCALLFILE=$VARDIR/ncall
DATE=`date +%d%H`

progname=${0##*/}
set -- $@		# have to split argument string !!!

# ----------------------------------------------------------------------

usage ()
{
	echo "usage: $progname -D device -d dest -s src"
	exit 1
}

ncall ()
{
	nfile=$1
	[ -f $nfile ] && read n < $nfile || n=0
	echo $(($n + 1)) > $nfile
	printf "%.4d" $n
}

# ----------------------------------------------------------------------

while getopts "D:d:s:" opt
do
	case $opt
	in
D)		DEVICE=$OPTARG	;;
d)		DEST=$OPTARG	;;
s)		SRC=$OPTARG	;;
	esac
done

[ -c "$DEVICE" -a -n "$DEST" -a -n "$SRC" ] || usage;

shift $(($OPTIND - 1))

# ----------------------------------------------------------------------

NCALL=`ncall $NCALLFILE`

echo "$progname: device $DEVICE destination $DEST source $SRC " >>$LOGFILE

{
	echo "Date:	"`date`
	echo "From:	\"$SRC\""
	echo "To:	\"$DEST\""
	echo
} >> $VARDIR/I.$NCALL.$DATE

# ----------------------------------------------------------------------

tellnumber ()
{
    number=$1
    digits=`echo $number | sed -e 's/\(.\)/\1 /g'`

    files=""
    for digit in $digits
    do
	files="$files $LIBDIR/$digit.al"
    done
    cat $files
}

# ----------------------------------------------------------------------

do_answer ()
{
	[ -f $LIBDIR/beep.al ] && cat $LIBDIR/beep.al
	[ -f $LIBDIR/msg.al ]  && cat $LIBDIR/msg.al
	[ -f $LIBDIR/beep.al ] && cat $LIBDIR/beep.al
} > $DEVICE

do_record ()
{
	cat $DEVICE > $VARDIR/R.$NCALL.$DATE
}

do_tell ()
{
	[ -f $LIBDIR/beep.al ] && cat $LIBDIR/beep.al
	[ -f $LIBDIR/msg.al ]  && cat $LIBDIR/msg.al
	tellnumber $SRC
	[ -f $LIBDIR/beep.al ] && cat $LIBDIR/beep.al
} > $DEVICE

# ----------------------------------------------------------------------

case $progname
in
answer)	do_answer		;;
record)	do_answer; do_record	;;
tell)	do_tell			;;
esac
