#! /bin/sh
#
# Copyright (C) 1995-2005 The Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# cvscheck - identify files added, changed, or removed 
#            in CVS working directory
#
# Contributed by Lowell Skoog <fluke!lowell@uunet.uu.net>
# 
# This program should be run in a working directory that has been
# checked out using CVS.  It identifies files that have been added,
# changed, or removed in the working directory, but not "cvs
# committed".  It also determines whether the files have been "cvs
# added" or "cvs removed".  For directories, it is only practical to
# determine whether they have been added.

name=cvscheck
changes=0

# If we can't run CVS commands in this directory
cvs status . > /dev/null 2>&1
if [ $? != 0 ] ; then

    # Bail out
    echo "$name: there is no version here; bailing out" 1>&2
    exit 1
fi

# Identify files added to working directory
for file in .* * ; do

    # Skip '.' and '..'
    if [ $file = '.' -o $file = '..' ] ; then
	continue
    fi

    # If a regular file
    if [ -f $file ] ; then
	if cvs status $file | grep -s '^From:[ 	]*New file' ; then
	    echo "file added:      $file - not CVS committed"
	    changes=`expr $changes + 1`
	elif cvs status $file | grep -s '^From:[ 	]*no entry for' ; then
	    echo "file added:      $file - not CVS added, not CVS committed"
	    changes=`expr $changes + 1`
	fi

    # Else if a directory
    elif [ -d $file -a $file != CVS.adm ] ; then

	# Move into it
	cd $file

	# If CVS commands don't work inside
	cvs status . > /dev/null 2>&1
	if [ $? != 0 ] ; then
	    echo "directory added: $file - not CVS added"
	    changes=`expr $changes + 1`
	fi

	# Move back up
	cd ..
    fi
done

# Identify changed files
changedfiles=`cvs diff | egrep '^diff' | awk '{print $3}'`
for file in $changedfiles ; do
    echo "file changed:    $file - not CVS committed"
    changes=`expr $changes + 1`
done

# Identify files removed from working directory
removedfiles=`cvs status | egrep '^File:[ 	]*no file' | awk '{print $4}'`

# Determine whether each file has been cvs removed
for file in $removedfiles ; do
    if cvs status $file | grep -s '^From:[ 	]*-' ; then
	echo "file removed:    $file - not CVS committed"
    else
	echo "file removed:    $file - not CVS removed, not CVS committed"
    fi
    changes=`expr $changes + 1`
done

exit $changes
