#!/bin/sh
# a quick sanity test for cvs.
#
# Copyright (C) 1992, 1993 Cygnus Support
#
# Original Author: K. Richard Pixley
# Last mod Thu Nov 4 16:37:08 PST 1993, by rich@sendai.cygnus.com

# 
# These commands are not covered at all.
#	admin

TESTDIR=/tmp/cvs-sanity

# "debugger"
#set -x

echo This test should produce no other output than this line.

# clean any old remnants
rm -rf ${TESTDIR}

# fixme: try things without -m.
# fixme: run this in a loop over "-Q", "-q", and "".
testcvs=$1
CVS="${testcvs} -Q"
OUTPUT=

LOGFILE=`pwd`/check.log
if test -f check.log; then mv check.log check.plog; fi

mkdir ${TESTDIR}
cd ${TESTDIR}

# so far so good.  Let's try something harder.

# this should die
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ${OUTPUT} ; then
	echo '***' failed test 1. ; exit 1
else
	true
fi

# this should still die
mkdir cvsroot
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ${OUTPUT} ; then
	echo '***' failed test 2. ; exit 1
else
	true
fi

# this should still die
mkdir cvsroot/CVSROOT
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ${OUTPUT} ; then
	echo '***' failed test 3. ; exit 1
else
	true
fi

# This one should work, although it should spit a warning.
mkdir tmp ; cd tmp
${CVS} -d `pwd`/../cvsroot co CVSROOT 2>> ${LOGFILE} ${OUTPUT}
cd .. ; rm -rf tmp

# This one should succeed.  No warnings.
touch cvsroot/CVSROOT/modules
mkdir tmp ; cd tmp
if ${CVS} -d `pwd`/../cvsroot co CVSROOT ${OUTPUT} ; then
	true
else
	echo '***' failed test 4. ; exit 1
fi

cd .. ; rm -rf tmp

# Try setting CVSROOT so we don't have to worry about it anymore.  (now that
# we've tested -d cvsroot.)
CVSROOT_FILENAME=`pwd`/cvsroot
CVSROOT=${CVSROOT_FILENAME} ; export CVSROOT
# This isn't exactly what we want since we would like to tell it to use ${CVS},
# rather than some random cvs from the PATH.
#CVSROOT=`hostname`:${CVSROOT_FILENAME} ; export CVSROOT

mkdir tmp ; cd tmp
if ${CVS} -d `pwd`/../cvsroot co CVSROOT ${OUTPUT} ; then
	true
else
	echo '***' failed test 5. ; exit 1
fi

cd .. ; rm -rf tmp

# start keeping history
touch ${CVSROOT_FILENAME}/CVSROOT/history

### The big loop
for what in basic0 basic1 basic2 basic3 rtags death import new ; do
	case $what in
	basic0) # Now, let's build something.
#		mkdir first-dir
		# this doesn't yet work, though I think maybe it should.  xoxorich.
#		if ${CVS} add first-dir ${OUTPUT} ; then
#			true
#		else
#			echo cvs does not yet add top level directories cleanly.
			mkdir ${CVSROOT_FILENAME}/first-dir
#		fi
#		rm -rf first-dir

		# check out an empty directory
		if ${CVS} co first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 6. ; exit 1
		fi

		# update the empty directory
		if ${CVS} update first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 7. ; exit 1
		fi

		# diff -u the empty directory
		if ${CVS} diff -u first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 8. ; exit 1
		fi

		# diff -c the empty directory
		if ${CVS} diff -c first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 9. ; exit 1
		fi

		# log the empty directory
		if ${CVS} log first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 10. ; exit 1
		fi

		# status the empty directory
		if ${CVS} status first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 11. ; exit 1
		fi

		# tag the empty directory
		if ${CVS} tag first first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 12. ; exit 1
		fi

		# rtag the empty directory
		if ${CVS} rtag empty first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 13. ; exit 1
		fi
		;;

	basic1) # first dive - add a files, first singly, then in a group.
		cd first-dir
		files=first-file
		for i in a b ; do
			for j in ${files} ; do
				echo $j > $j
			done

			for do in add rm ; do
				for j in ${do} "commit -m test" ; do
					# ${do}
					if ${CVS} $j ${files} ${OUTPUT} >> ${LOGFILE} 2>&1; then
						true
					else
						echo '***' failed test 14-${do}-$j. ; exit 1
					fi

					# update it.
					if [ "${do}" = "rm" -a "$j" != "commit -m test" ] || ${CVS} update ${files} ; then
						true
					else
						echo '***' failed test 15-${do}-$j. ; exit 1
					fi

					# update all.
					if ${CVS} update ${OUTPUT} ; then
						true
					else
						echo '***' failed test 16-${do}-$j. ; exit 1
					fi

					# status all.
					if ${CVS} status ${OUTPUT} >> ${LOGFILE}; then
						true
					else
						echo '***' failed test 17-${do}-$j. ; exit 1
					fi

		# fixme: this one doesn't work yet for added files.
					# log all.
					if ${CVS} log ${OUTPUT} >> ${LOGFILE}; then
						true
					else
						echo '***' failed test 18-${do}-$j. #; exit 1
					fi

					if test "x${do}-$j" = "xadd-add" || test "x${do}-$j" = "xrm-rm" ; then
					  true
					else
					  # diff -c all
					  if ${CVS} diff -c ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
					    true
					  else
					    echo '***' failed test 19-${do}-$j. # FIXME; exit 1
					  fi

					  # diff -u all
					  if ${CVS} diff -u ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
					    true
					  else
					    echo '***' failed test 20-${do}-$j. # FIXME; exit 1
					  fi
					fi

					cd ..
					# update all.
					if ${CVS} update ${OUTPUT} ; then
						true
					else
						echo '***' failed test 21-${do}-$j. ; exit 1
					fi

					# log all.
		# fixme: doesn't work right for added files.
					if ${CVS} log first-dir ${OUTPUT} >> ${LOGFILE}; then
						true
					else
						echo '***' failed test 22-${do}-$j. #; exit 1
					fi

					# status all.
					if ${CVS} status first-dir ${OUTPUT} >> ${LOGFILE}; then
						true
					else
						echo '***' failed test 23-${do}-$j. ; exit 1
					fi

					# update all.
					if ${CVS} update first-dir ${OUTPUT} ; then
						true
					else
						echo '***' failed test 24-${do}-$j. ; exit 1
					fi

					if test "x${do}-$j" = "xadd-add" || test "x${do}-$j" = "xrm-rm" ; then
					  true
					else
					  # diff all
					  if ${CVS} diff -u ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
					    true
					  else
					    echo '***' failed test 25-${do}-$j. # FIXME; exit 1
					  fi

					  # diff all
					  if ${CVS} diff -u first-dir ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
					    true
					  else
					    echo '***' failed test 26-${do}-$j. # FIXME; exit 1
					  fi
					fi

					# update all.
					if ${CVS} co first-dir ${OUTPUT} ; then
						true
					else
						echo '***' failed test 27-${do}-$j. ; exit 1
					fi

					cd first-dir
				done # j
				rm -f ${files}
			done # do

			files="file2 file3 file4 file5"
		done
		if ${CVS} tag first-dive ${OUTPUT} ; then
			true
		else
			echo '***' failed test 28. ; exit 1
		fi
		cd ..
		;;

	basic2) # second dive - add bunch o' files in bunch o' added directories
		for i in first-dir dir1 dir2 dir3 dir4 ; do
			if [ ! -d $i ] ; then
				mkdir $i
				if ${CVS} add $i ${OUTPUT} >> ${LOGFILE}; then
					true
				else
					echo '***' failed test 29-$i. ; exit 1
				fi
			fi

			cd $i

			for j in file6 file7 file8 file9 file10 file11 file12 file13; do
				echo $j > $j
			done

			if ${CVS} add file6 file7 file8 file9 file10 file11 file12 file13 ${OUTPUT} 2>> ${LOGFILE}; then
				true
			else
				echo '***' failed test 30-$i-$j. ; exit 1
			fi
		done
		cd ../../../../..
		if ${CVS} update first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 31. ; exit 1
		fi

		# fixme: doesn't work right for added files.
		if ${CVS} log first-dir ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 32. # ; exit 1
		fi

		if ${CVS} status first-dir ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 33. ; exit 1
		fi

#		if ${CVS} diff -u first-dir ${OUTPUT}  >> ${LOGFILE} || [ $? = 1 ] ; then
#			true
#		else
#			echo '***' failed test 34. # ; exit 1
#		fi

		if ${CVS} ci -m "second dive" first-dir ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 35. ; exit 1
		fi

		if ${CVS} tag second-dive first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 36. ; exit 1
		fi
		;;

	basic3) # third dive - in bunch o' directories, add bunch o' files, delete some, change some.
		for i in first-dir dir1 dir2 dir3 dir4 ; do
			cd $i

			# modify some files
			for j in file6 file8 file10 file12 ; do
				echo $j >> $j
			done

			# delete some files
			rm file7 file9 file11 file13

			if ${CVS} rm file7 file9 file11 file13 ${OUTPUT} 2>> ${LOGFILE}; then
				true
			else
				echo '***' failed test 37-$i. ; exit 1
			fi

			# and add some new ones
			for j in file14 file15 file16 file17 ; do
				echo $j > $j
			done

			if ${CVS} add file14 file15 file16 file17 ${OUTPUT} 2>> ${LOGFILE}; then
				true
			else
				echo '***' failed test 38-$i. ; exit 1
			fi
		done
		cd ../../../../..
		if ${CVS} update first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 39. ; exit 1
		fi

		# fixme: doesn't work right for added files
		if ${CVS} log first-dir ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 40. # ; exit 1
		fi

		if ${CVS} status first-dir ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 41. ; exit 1
		fi

#		if ${CVS} diff -u first-dir ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
#			true
#		else
#			echo '***' failed test 42. # ; exit 1
#		fi

		if ${CVS} ci -m "third dive" first-dir ${OUTPUT} >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 43. ; exit 1
		fi

		if ${CVS} tag third-dive first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 44. ; exit 1
		fi

		# Hmm...  fixme.
#		if ${CVS} release first-dir ${OUTPUT} ; then
#			true
#		else
#			echo '***' failed test 45. # ; exit 1
#		fi

		# end of third dive
		rm -rf first-dir
		;;

	rtags) # now try some rtags
		# rtag HEADS
		if ${CVS} rtag rtagged-by-head first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 46. ; exit 1
		fi

		# tag by tag
		if ${CVS} rtag -r rtagged-by-head rtagged-by-tag first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 47. ; exit 1
		fi

		# tag by revision
		if ${CVS} rtag -r1.1 rtagged-by-revision first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 48. ; exit 1
		fi

		# rdiff by revision
		if ${CVS} rdiff -r1.1 -rrtagged-by-head first-dir ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
			true
		else
			echo '***' failed test 49. ; exit 1
		fi

		# now export by rtagged-by-head and rtagged-by-tag and compare.
		rm -rf first-dir
		if ${CVS} export -r rtagged-by-head first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 50. ; exit 1
		fi

		mv first-dir 1dir
		if ${CVS} export -r rtagged-by-tag first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 51. ; exit 1
		fi

		if diff -c -r 1dir first-dir ; then
			true
		else
			echo '***' failed test 52. ; exit 1
		fi
		rm -rf 1dir first-dir

		# For some reason, this command has stopped working and hence much of this sequence is currently off.
		# export by revision vs checkout by rtagged-by-revision and compare.
#		if ${CVS} export -r1.1 first-dir ${OUTPUT} ; then
#			true
#		else
#			echo '***' failed test 53. # ; exit 1
#		fi
		# note sidestep below
		#mv first-dir 1dir

		if ${CVS} co -rrtagged-by-revision first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 54. ; exit 1
		fi
		# fixme: this is here temporarily to sidestep test 53.
		ln -s first-dir 1dir

		# directory copies are done in an oblique way in order to avoid a bug in sun's tmp filesystem.
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - * | (cd ../first-dir.cpy ; tar xf -))

		if diff --exclude=CVS -c -r 1dir first-dir ; then
			true
		else
			echo '***' failed test 55. ; exit 1
		fi

		# interrupt, while we've got a clean 1.1 here, let's import it into another tree.
		cd 1dir
		if ${CVS} import -m "first-import" second-dir first-immigration immigration1 immigration1_0 ${OUTPUT} ; then
			true
		else
			echo '***' failed test 56. ; exit 1
		fi
		cd ..

		if ${CVS} export -r HEAD second-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 57. ; exit 1
		fi

		if diff --exclude=CVS -c -r first-dir second-dir ; then
			true
		else
			echo '***' failed test 58. ; exit 1
		fi

		rm -rf 1dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - * | (cd ../first-dir ; tar xf -))

		# update the top, cancelling sticky tags, retag, update other copy, compare.
		cd first-dir
		if ${CVS} update -A -l *file* ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 59. ; exit 1
		fi

		# If we don't delete the tag first, cvs won't retag it.
		# This would appear to be a feature.
		if ${CVS} tag -l -d rtagged-by-revision ${OUTPUT} ; then
			true
		else
			echo '***' failed test 60a. ; exit 1
		fi
		if ${CVS} tag -l rtagged-by-revision ${OUTPUT} ; then
			true
		else
			echo '***' failed test 60b. ; exit 1
		fi

		cd .. ; mv first-dir 1dir
		mv first-dir.cpy first-dir ; cd first-dir
		if ${CVS} diff -u ${OUTPUT} >> ${LOGFILE} || [ $? = 1 ] ; then
			true
		else
			echo '***' failed test 61. ; exit 1
		fi

		if ${CVS} update ${OUTPUT} ; then
			true
		else
			echo '***' failed test 62. ; exit 1
		fi

		cd ..

# Haven't investigated why this is failing.
#		if diff --exclude=CVS -c -r 1dir first-dir ; then
#			true
#		else
#			echo '***' failed test 63. # ; exit 1
#		fi
		rm -rf 1dir first-dir

		if ${CVS} his -e -a ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 64. ; exit 1
		fi
		;;

	death) # next dive.  test death support.
		rm -rf ${CVSROOT_FILENAME}/first-dir
		mkdir  ${CVSROOT_FILENAME}/first-dir
		if ${CVS} co first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 65 ; exit 1
		fi

		cd first-dir

		# add a file.
		touch file1
		if ${CVS} add file1 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 66 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 67 ; exit 1
		fi

		# remove
		rm file1
		if ${CVS} rm file1 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 68 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} ; then
			true
		else
			echo '***' failed test 69 ; exit 1
		fi

		# add again and create second file
		touch file1 file2
		if ${CVS} add file1 file2 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 70 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 71 ; exit 1
		fi

		# log
		if ${CVS} log file1 ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 72 ; exit 1
		fi


		# branch1
		if ${CVS} tag -b branch1 ${OUTPUT} ; then
			true
		else
			echo '***' failed test 73 ; exit 1
		fi

		# and move to the branch.
		if ${CVS} update -r branch1 ${OUTPUT} ; then
			true
		else
			echo '***' failed test 74 ; exit 1
		fi

		# add a file in the branch
		echo line1 from branch1 >> file3
		if ${CVS} add file3 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 75 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 76 ; exit 1
		fi

		# remove
		rm file3
		if ${CVS} rm file3 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 77 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} ; then
			true
		else
			echo '***' failed test 78 ; exit 1
		fi

		# add again
		echo line1 from branch1 >> file3
		if ${CVS} add file3 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 79 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 80 ; exit 1
		fi

		# change the first file
		echo line2 from branch1 >> file1

		# commit
		if ${CVS} ci -m test ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 81 ; exit 1
		fi

		# remove the second
		rm file2
		if ${CVS} rm file2 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 82 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} ; then
			true
		else
			echo '***' failed test 83 ; exit 1
		fi

		# back to the trunk.
		if ${CVS} update -A ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 84 ; exit 1
		fi

		if [ -f file3 ] ; then
			echo '***' failed test 85 ; exit 1
		else
			true
		fi

		# join
		if ${CVS} update -j branch1 ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 86 ; exit 1
		fi

		if [ -f file3 ] ; then
			true
		else
			echo '***' failed test 87 ; exit 1
		fi

		# update
		if ${CVS} update ${OUTPUT} ; then
			true
		else
			echo '***' failed test 88 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 89 ; exit 1
		fi

		# remove first file.
		rm file1
		if ${CVS} rm file1 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 90 ; exit 1
		fi

		# commit
		if ${CVS} ci -m test ${OUTPUT} ; then
			true
		else
			echo '***' failed test 91 ; exit 1
		fi

		if [ -f file1 ] ; then
			echo '***' failed test 92 ; exit 1
		else
			true
		fi

		# back to branch1
		if ${CVS} update -r branch1 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 93 ; exit 1
		fi

		if [ -f file1 ] ; then
			true
		else
			echo '***' failed test 94 ; exit 1
		fi

		# and join
		if ${CVS} update -j HEAD ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 95 ; exit 1
		fi

		cd .. ; rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		;;

	import) # test death after import
		# import
		mkdir import-dir ; cd import-dir

		for i in 1 2 3 4 ; do
			echo imported file"$i" > imported-file"$i"
		done

		if ${CVS} import -m first-import first-dir vendor-branch junk-1_0 ${OUTPUT} ; then
			true
		else
			echo '***' failed test 96 ; exit 1
		fi
		cd ..

		# co
		if ${CVS} co first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 97 ; exit 1
		fi

		cd first-dir
		for i in 1 2 3 4 ; do
			if [ -f imported-file"$i" ] ; then
				true
			else
				echo '***' failed test 98-$i ; exit 1
			fi
		done

		# remove
		rm imported-file1
		if ${CVS} rm imported-file1 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 99 ; exit 1
		fi

		# change
		# this sleep is significant.  Otherwise, on some machines, things happen so
		# fast that the file mod times do not differ.
		sleep 1
		echo local-change >> imported-file2

		# commit
		if ${CVS} ci -m local-changes ${OUTPUT} >> ${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 100 ; exit 1
		fi

		# log
		if ${CVS} log imported-file1 | grep '1.1.1.2 (dead)' ${OUTPUT} ; then
			echo '***' failed test 101 ; exit 1
		else
			true
		fi

		# update into the vendor branch.
		if ${CVS} update -rvendor-branch ${OUTPUT} ; then
			true
		else
			echo '***' failed test 102 ; exit 1
		fi

		# remove file4 on the vendor branch
		rm imported-file4

		if ${CVS} rm imported-file4 ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 103 ; exit 1
		fi

		# commit
		if ${CVS} ci -m vendor-removed imported-file4 ${OUTPUT} ; then
			true
		else
			echo '***' failed test 104 ; exit 1
		fi

		# update to main line
		if ${CVS} update -A ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 105 ; exit 1
		fi

		# second import - file4 deliberately unchanged
		cd ../import-dir
		for i in 1 2 3 ; do
			echo rev 2 of file $i >> imported-file"$i"
		done

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0 ${OUTPUT} ; then
			true
		else
			echo '***' failed test 106 ; exit 1
		fi
		cd ..

		# co
		if ${CVS} co first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 107 ; exit 1
		fi

		cd first-dir

		if [ -f imported-file1 ] ; then
			echo '***' failed test 108 ; exit 1
		else
			true
		fi

		for i in 2 3 ; do
			if [ -f imported-file"$i" ] ; then
				true
			else
				echo '***' failed test 109-$i ; exit 1
			fi
		done

		# check vendor branch for file4
		if ${CVS} update -rvendor-branch ${OUTPUT} ; then
			true
		else
			echo '***' failed test 110 ; exit 1
		fi

		if [ -f imported-file4 ] ; then
			true
		else
			echo '***' failed test 111 ; exit 1
		fi

		# update to main line
		if ${CVS} update -A ${OUTPUT} 2>> ${LOGFILE}; then
			true
		else
			echo '***' failed test 112 ; exit 1
		fi

		cd ..

		if ${CVS} co -jjunk-1_0 -jjunk-2_0 first-dir ${OUTPUT} >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 113 ; exit 1
		fi

		cd first-dir

		if [ -f imported-file1 ] ; then
			echo '***' failed test 114 ; exit 1
		else
			true
		fi

		for i in 2 3 ; do
			if [ -f imported-file"$i" ] ; then
				true
			else
				echo '***' failed test 115-$i ; exit 1
			fi
		done

		if cat imported-file2 | grep '====' ${OUTPUT} >> ${LOGFILE}; then
			true
		else
			echo '***' failed test 116 ; exit 1
		fi
		cd .. ; rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		;;

	new) # look for stray "no longer pertinent" messages.
		rm -rf first-dir ${CVSROOT_FILENAME}/first-dir
		mkdir ${CVSROOT_FILENAME}/first-dir

		if ${CVS} co first-dir ${OUTPUT} ; then
			true
		else
			echo '***' failed test 117 ; exit 1
		fi

		cd first-dir
		touch a

		if ${CVS} add a ${OUTPUT} 2>>${LOGFILE}; then
			true
		else
			echo '***' failed test 118 ; exit 1
		fi

		if ${CVS} ci -m added ${OUTPUT} >>${LOGFILE} 2>&1; then
			true
		else
			echo '***' failed test 119 ; exit 1
		fi

		rm a

		if ${CVS} rm a ${OUTPUT} 2>>${LOGFILE}; then
			true
		else
			echo '***' failed test 120 ; exit 1
		fi

		if ${CVS} ci -m removed ${OUTPUT} ; then
			true
		else
			echo '***' failed test 121 ; exit 1
		fi

		if ${CVS} update -A ${OUTPUT} 2>&1 | grep longer ; then
			echo '***' failed test 122 ; exit 1
		else
			true
		fi

		if ${CVS} update -rHEAD 2>&1 | grep longer ; then
			echo '***' failed test 123 ; exit 1
		else
			true
		fi

		cd .. ; rm -rf first-dir ; rm -rf ${CVSROOT_FILENAME}/first-dir
		;;

	*) echo Ooops - $what ;;
	esac
done

echo Ok.

# Local Variables:
# fill-column: 131
# End:

# end of sanity.sh
