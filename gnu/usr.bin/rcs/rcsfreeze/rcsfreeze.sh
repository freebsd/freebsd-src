#! /bin/sh

# rcsfreeze - assign a symbolic revision number to a configuration of RCS files

#	$Id: rcsfreeze.sh,v 1.3 1996/03/12 21:42:40 wosch Exp $

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

PATH=/bin:/usr/bin:$PATH
export PATH

DATE=`date` || exit
# Check whether we have an RCS subdirectory, so we can have the right
# prefix for our paths.
if test -d RCS
then RCSDIR=RCS/ EXT=
else RCSDIR= EXT=,v
fi

# Version number stuff, log message file
VERSIONFILE=${RCSDIR}.rcsfreeze.ver
LOGFILE=${RCSDIR}.rcsfreeze.log
# Initialize, rcsfreeze never run before in the current directory
test -r $VERSIONFILE || { echo 0 >$VERSIONFILE && >>$LOGFILE; } || exit

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
rm -f $TMPLOG &&

# Now the real work begins by assigning a symbolic revision number
# to each rcs file.  Take the most recent version on the default branch.

# If there are any .*,v files, throw them in too.
# But ignore RCS/.* files that do not end in ,v.
DOTFILES=
for DOTFILE in ${RCSDIR}.*,v
do
	if test -f "$DOTFILE"
	then
		DOTFILES="${RCSDIR}.*,v"
		break
	fi
done

exec rcs -q -n$SYMREVNAME: ${RCSDIR}*$EXT $DOTFILES
