#! /bin/sh
#
# Copyright (C) 1997-2005 The Free Software Foundation, Inc.
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
#	cvs2vendor - move revsisions from files in A to files in B
# 
# The primary reason for this script is to move deltas from a
# non-vendor branched repository onto a fresh vendor branched one,
# skipping the initial checkin in assumption that it is the same in
# both repositories.  This way you can take a project that was moved
# into CVS without the benefit of the vendor branch and for all
# intents and purposes add the vendor branch underneath the existing
# deltas.
# 
# This script is also a decent example of repository maintenance using
# raw RCS commands (if I do say so myself! ;-).
# 
# Tags are preserved.
# 
# The timestamp of the initial vendor branch revision will be adjusted
# to be the same as the 1.1 revision of each source file.
# 
# Extra branches in the source directory will cause breakage.
# 
# Intermediate files are created in the current working directory
# where this script is started.
# 
# Written by Greg A. Woods <woods@planix.com>, based on rcs2sccs
# (retains some of the rlog parsing from it).
# 
# The copyright is in the Public Domain.
#

if [ $# -ne 2 ]; then
	echo USAGE: $0 srcdir dstdir
	exit 2
fi
tsrcdir=$1
tdstdir=$2

revfile=/tmp/cvs2vendor_$$_rev
rm -f $revfile

commentfile=/tmp/cvs2vendor_$$_comment
rm -f $commentfile

if sort -k 1,1 /dev/null 2>/dev/null
then sort_each_field='-k 1 -k 2 -k 3 -k 4 -k 5 -k 6 -k 7 -k 8 -k 9'
else sort_each_field='+0 +1 +2 +3 +4 +5 +6 +7 +8'
fi

srcdirs=`cd $tsrcdir && find . -type d -print | sed 's~^\.[/]*~~'`

# the "" is a trick to get $tsrcdir itself without resorting to '.'
for ldir in "" $srcdirs; do

	srcdir=$tsrcdir/$ldir
	dstdir=$tdstdir/$ldir

	# Loop over every RCS file in srcdir
	#
	for vfile in $srcdir/*,v; do
		# get rid of the ",v" at the end of the name
		file=`echo $vfile | sed -e 's/,v$//'`
		bfile=`basename $file`

		if [ ! -d $dstdir ]; then
			echo "making locally added directory $dstdir"
			mkdir -p $dstdir
		fi
		if [ ! -f $dstdir/$bfile,v ]; then
			echo "copying locally added file $dstdir/$bfile ..."
			cp $vfile $dstdir
			continue;
		fi

		# work on each rev of that file in ascending order
		rlog $file | grep "^revision [0-9][0-9]*\." | awk '{print $2}' | sed -e 's/\./ /g' | sort -n -u $sort_each_field | sed -e 's/ /./g' > $revfile

		for rev in `cat $revfile`; do

			case "$rev" in
			1.1)
				newdate=`rlog -r$rev $file | grep "^date: " | awk '{printf("%s.%s\n",$2,$3); exit}' | sed -e 's~/~.~g' -e 's/:/./g' -e 's/;//' -e 's/^19//'`
				olddate=`rlog -r1.1.1.1 $dstdir/$bfile | grep "^date: " | awk '{printf("%s.%s\n",$2,$3); exit}' | sed -e 's~/~.~g' -e 's/:/./g' -e 's/;//' -e 's/^19//'`
				sed "s/$olddate/$newdate/" < $dstdir/$bfile,v > $dstdir/$bfile.x
				mv -f $dstdir/$bfile.x $dstdir/$bfile,v
				chmod -w $dstdir/$bfile,v
				symname=`rlog -h $file | sed -e '1,/^symbolic names:/d' -e 's/[ 	]*//g' | awk -F: '$2 == "'"$rev"'" {printf("-n%s:1.1.1.1\n",$1)}'`
				if [ -n "$symname" ]; then
					echo "tagging $file with $symname ..."
					rcs $symname $dstdir/$bfile,v
					if [ $? != 0 ]; then
						echo ERROR - rcs $symname $dstdir/$bfile,v
						exit 1
					fi
				fi
				continue			# skip first rev....
				;;
			esac

			# get a lock on the destination local branch tip revision
			co -r1 -l $dstdir/$bfile
			if [ $? != 0 ]; then
				echo ERROR - co -r1 -l $dstdir/$bfile
				exit 1
			fi
			rm -f $dstdir/$bfile

			# get file into current dir and get stats
			date=`rlog -r$rev $file | grep "^date: " | awk '{printf("%s %s\n",$2,$3); exit}' | sed -e 's/;//'`
			author=`rlog -r$rev $file | grep "^date: " | awk '{print $5; exit}' | sed -e 's/;//'`

			symname=`rlog -h $file | sed -e '1,/^symbolic names:/d' -e 's/[ 	]*//g' | awk -F: '$2 == "'"$rev"'" {printf("-n%s\n",$1)}'`

			rlog -r$rev $file | sed -e '/^branches: /d' -e '1,/^date: /d' -e '/^===========/d' | awk '{if ((total += length($0) + 1) < 510) print $0}' > $commentfile

			echo "==> file $file, rev=$rev, date=$date, author=$author $symname"

			co -p -r$rev $file > $bfile
			if [ $? != 0 ]; then
				echo ERROR - co -p -r$rev $file
				exit 1
			fi

			# check file into vendor repository...
			ci -f -m"`cat $commentfile`" -d"$date" $symname -w"$author" $bfile $dstdir/$bfile,v
			if [ $? != 0 ]; then
				echo ERROR - ci -f -m"`cat $commentfile`" -d"$date" $symname -w"$author" $bfile $dstdir/$bfile,v
				exit 1
			fi
			rm -f $bfile

			# set the default branch to the trunk...
			# XXX really only need to do this once....
			rcs -b1 $dstdir/$bfile
			if [ $? != 0 ]; then
				echo ERROR - rcs -b1 $dstdir/$bfile
				exit 1
			fi
		done
	done
done

echo cleaning up...
rm -f $commentfile
echo "       Conversion Completed Successfully"

exit 0
