#! /bin/sh

# rcsfreeze - assign a symbolic revision number to a configuration of RCS files

#	$Id: rcsfreeze.sh,v 4.4 1991/04/21 11:58:24 eggert Exp $

#       The idea is to run rcsfreeze each time a new version is checked
#       in. A unique symbolic revision number (C_[number], where number
#       is increased each time rcsfreeze is run) is then assigned to the most
#       recent revision of each RCS file of the main trunk.
#
#       If the command is invoked with an argument, then this
#       argument is used as the symbolic name to freeze a configuration.
#       The unique identifier is still generated
#       and is listed in the log file but it will not appear as
#       part of the symbolic revision name in the actual RCS file.
#
#       A log message is requested from the user which is saved for future
#       references.
#
#       The shell script works only on all RCS files at one time.
#       It is important that all changed files are checked in (there are
#       no precautions against any error in this respect).
#       file names:
#       {RCS/}.rcsfreeze.ver	version number
#       {RCS/}.rscfreeze.log	log messages, most recent first

PATH=/usr/gnu/bin:/usr/local/bin:/bin:/usr/bin:/usr/ucb:$PATH
export PATH

DATE=`date` || exit
# Check whether we have an RCS subdirectory, so we can have the right
# prefix for our paths.
if [ -d RCS ]
then RCSDIR=RCS/
else RCSDIR=
fi

# Version number stuff, log message file
VERSIONFILE=${RCSDIR}.rcsfreeze.ver
LOGFILE=${RCSDIR}.rcsfreeze.log
# Initialize, rcsfreeze never run before in the current directory
[ -r $VERSIONFILE ] || { echo 0 >$VERSIONFILE && >>$LOGFILE; } || exit

# Get Version number, increase it, write back to file.
VERSIONNUMBER=`cat $VERSIONFILE` &&
VERSIONNUMBER=`expr $VERSIONNUMBER + 1` &&
echo $VERSIONNUMBER >$VERSIONFILE || exit

# Symbolic Revision Number
SYMREV=C_$VERSIONNUMBER
# Allow the user to give a meaningful symbolic name to the revision.
SYMREVNAME=${1-$SYMREV}
echo >&2 "rcsfreeze: symbolic revision number computed: \"${SYMREV}\"
rcsfreeze: symbolic revision number used:     \"${SYMREVNAME}\"
rcsfreeze: the two differ only when rcsfreeze invoked with argument
rcsfreeze: give log message, summarizing changes (end with EOF or single '.')" \
	|| exit

# Stamp the logfile. Because we order the logfile the most recent
# first we will have to save everything right now in a temporary file.
TMPLOG=/tmp/rcsfrz$$
trap 'rm -f $TMPLOG; exit 1' 1 2 13 15
# Now ask for a log message, continously add to the log file
(
	echo "Version: $SYMREVNAME($SYMREV), Date: $DATE
-----------" || exit
	while read MESS
	do
		case $MESS in
		.) break
		esac
		echo "	$MESS" || exit
	done
	echo "-----------
" &&
	cat $LOGFILE
) >$TMPLOG &&

# combine old and new logfiles
cp $TMPLOG $LOGFILE &&
rm -f $TMPLOG || exit
trap 1 2 13 15

# Now the real work begins by assigning a symbolic revision number
# to each rcs file. Take the most recent version of the main trunk.

status=

for FILE in ${RCSDIR}*
do
#   get the revision number of the most recent revision
    HEAD=`rlog -h $FILE` &&
	REV=`echo "$HEAD" | sed -n 's/^head:[ 	]*//p'` &&
#   assign symbolic name to it.
    echo >&2 "rcsfreeze: $REV $FILE" &&
    rcs -q -n$SYMREVNAME:$REV $FILE || status=$?
done

exit $status
