#! /bin/sh
:
#	sanity.sh -- a growing sanity test for cvs.
#
#ident	"$CVSid$"
#
# Copyright (C) 1992, 1993 Cygnus Support
#
# Original Author: K. Richard Pixley

# usage: sanity.sh [-r] @var{cvs-to-test} @var{tests-to-run}
# -r means to test remote instead of local cvs.
# @var{tests-to-run} are the names of the tests to run; if omitted run all
# tests.

# See TODO list at end of file.

# required to make this script work properly.
unset CVSREAD

TESTDIR=/tmp/cvs-sanity

# "debugger"
#set -x

echo 'This test should produce no other output than this line, and a final "OK".'

if test x"$1" = x"-r"; then
	shift
	remote=yes
else
	remote=no
fi

# Use full path for CVS executable, so that CVS_SERVER gets set properly
# for remote.
case $1 in
/*)
	testcvs=$1
	;;
*)
	testcvs=`pwd`/$1
	;;
esac

shift

# Use full path for mkmodules, so that the right one will be invoked
#
testmkmodules=`pwd`/mkmodules

# FIXME: try things (what things? checkins?) without -m.
#
# Some of these tests are written to expect -Q.  But testing with
# -Q is kind of bogus, it is not the way users actually use CVS (usually).
# So new tests probably should invoke ${testcvs} directly, rather than ${CVS}.
# and then they've obviously got to do something with the output....
#
CVS="${testcvs} -Q -f"

LOGFILE=`pwd`/check.log

# Save the previous log in case the person running the tests decides
# they want to look at it.  The extension ".plog" is chosen for consistency
# with dejagnu.
if test -f check.log; then
	mv check.log check.plog
fi

# clean any old remnants
rm -rf ${TESTDIR}
mkdir ${TESTDIR}
cd ${TESTDIR}

# Remaining arguments are the names of tests to run.
#
# FIXME: not all combinations are possible; basic3 depends on files set up
# by previous tests, for example.  This should be changed.
# The goal is that tests can be run in manageably-sized chunks, so
# that one can quickly get a result from a cvs or testsuite change,
# and to facilitate understanding the tests.

if test x"$*" = x; then
	tests="basic0 basic1 basic2 basic3 rtags death import new conflicts modules mflag errmsg1"
else
	tests="$*"
fi

# this should die
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
	echo "FAIL: test 1" | tee -a ${LOGFILE}
	exit 1
else
	echo "PASS: test 1" >>${LOGFILE}
fi

# this should still die
mkdir cvsroot
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
	echo "FAIL: test 2" | tee -a ${LOGFILE}
	exit 1
else
	echo "PASS: test 2" >>${LOGFILE}
fi

# this should still die
mkdir cvsroot/CVSROOT
if ${CVS} -d `pwd`/cvsroot co cvs-sanity 2>> ${LOGFILE} ; then
	echo "FAIL: test 3" | tee -a ${LOGFILE}
	exit 1
else
	echo "PASS: test 3" >>${LOGFILE}
fi

# This one should work, although it should spit a warning.
mkdir tmp ; cd tmp
${CVS} -d `pwd`/../cvsroot co CVSROOT 2>> ${LOGFILE}
cd .. ; rm -rf tmp

# set up a minimal modules file...
echo "CVSROOT		-i ${testmkmodules} CVSROOT" > cvsroot/CVSROOT/modules

# This one should succeed.  No warnings.
mkdir tmp ; cd tmp
if ${CVS} -d `pwd`/../cvsroot co CVSROOT ; then
	echo "PASS: test 4" >>${LOGFILE}
else
	echo "FAIL: test 4" | tee -a ${LOGFILE}
	exit 1
fi

if echo "yes" | ${CVS} -d `pwd`/../cvsroot release -d CVSROOT ; then
	echo "PASS: test 4.5" >>${LOGFILE}
else
	echo "FAIL: test 4.5" | tee -a ${LOGFILE}
	exit 1
fi
# this had better be empty
cd ..; rmdir tmp
if [ -d tmp ] ; then
	echo "FAIL: test 4.75" | tee -a ${LOGFILE}
	exit 1
fi

# a simple function to compare directory contents
#
# BTW, I don't care any more -- if you don't have a /bin/sh that handles
# shell functions, well get one.
#
# Returns: ISDIFF := true|false
#
directory_cmp ()
{
	OLDPWD=`pwd`
	DIR_1=$1
	DIR_2=$2
	ISDIFF=false

	cd $DIR_1
	find . -print | fgrep -v /CVS | sort > /tmp/dc$$d1

	# go back where we were to avoid symlink hell...
	cd $OLDPWD
	cd $DIR_2
	find . -print | fgrep -v /CVS | sort > /tmp/dc$$d2

	if diff /tmp/dc$$d1 /tmp/dc$$d2 >/dev/null 2>&1
	then
		:
	else
		ISDIFF=true
		return
	fi
	cd $OLDPWD
	while read a
	do
		if [ -f $DIR_1/"$a" ] ; then
			cmp -s $DIR_1/"$a" $DIR_2/"$a"
			if [ $? -ne 0 ] ; then
				ISDIFF=true
			fi
		fi
	done < /tmp/dc$$d1
### FIXME:
###	rm -f /tmp/dc$$*
}

# so much for the setup.  Let's try something harder.

# Try setting CVSROOT so we don't have to worry about it anymore.  (now that
# we've tested -d cvsroot.)
CVSROOT_DIRNAME=${TESTDIR}/cvsroot
CVSROOT=${CVSROOT_DIRNAME} ; export CVSROOT
if test "x$remote" = xyes; then
	CVSROOT=`hostname`:${CVSROOT_DIRNAME} ; export CVSROOT
	# Use rsh so we can test it without having to muck with inetd or anything 
	# like that.  Also needed to get CVS_SERVER to work.
	CVS_CLIENT_PORT=-1; export CVS_CLIENT_PORT
	CVS_SERVER=${testcvs}; export CVS_SERVER
fi

mkdir tmp ; cd tmp
if ${CVS} co CVSROOT ; then
	if [ -r CVSROOT/CVS/Entries ] ; then
		echo "PASS: test 5" >>${LOGFILE}
	else
		echo "FAIL: test 5" | tee -a ${LOGFILE}
		exit 1
	fi
else
	echo "FAIL: test 5" | tee -a ${LOGFILE}; exit 1
fi

if echo "yes" | ${CVS} release -d CVSROOT ; then
	echo "PASS: test 5.5" >>${LOGFILE}
else
	echo "FAIL: test 5.5" | tee -a ${LOGFILE}
	exit 1
fi
# this had better etmpy now...
cd ..; rmdir tmp
if [ -d tmp ] ; then
	echo "FAIL: test 5.75" | tee -a ${LOGFILE}
	exit 1
fi

# start keeping history
touch ${CVSROOT_DIRNAME}/CVSROOT/history

### The big loop
for what in $tests; do
	case $what in
	basic0) # Now, let's build something.
#		mkdir first-dir
		# this doesn't yet work, though I think maybe it should.  xoxorich.
#		if ${CVS} add first-dir ; then
#			true
#		else
#			echo cvs does not yet add top level directories cleanly.
			mkdir ${CVSROOT_DIRNAME}/first-dir
#		fi
#		rm -rf first-dir

		# check out an empty directory
		if ${CVS} co first-dir ; then
		  if [ -r first-dir/CVS/Entries ] ; then
		    echo "PASS: test 6" >>${LOGFILE}
		  else
		    echo "FAIL: test 6" | tee -a ${LOGFILE}; exit 1
		  fi
		else
		  echo "FAIL: test 6" | tee -a ${LOGFILE}; exit 1
		fi

		# update the empty directory
		if ${CVS} update first-dir ; then
		  echo "PASS: test 7" >>${LOGFILE}
		else
		  echo "FAIL: test 7" | tee -a ${LOGFILE}; exit 1
		fi

		# diff -u the empty directory
		if ${CVS} diff -u first-dir ; then
		  echo "PASS: test 8" >>${LOGFILE}
		else
		  echo "FAIL: test 8" | tee -a ${LOGFILE}; exit 1
		fi

		# diff -c the empty directory
		if ${CVS} diff -c first-dir ; then
		  echo "PASS: test 9" >>${LOGFILE}
		else
		  echo "FAIL: test 9" | tee -a ${LOGFILE}; exit 1
		fi

		# log the empty directory
		if ${CVS} log first-dir ; then
		  echo "PASS: test 10" >>${LOGFILE}
		else
		  echo "FAIL: test 10" | tee -a ${LOGFILE}; exit 1
		fi

		# status the empty directory
		if ${CVS} status first-dir ; then
		  echo "PASS: test 11" >>${LOGFILE}
		else
		  echo "FAIL: test 11" | tee -a ${LOGFILE}; exit 1
		fi

		# tag the empty directory
		if ${CVS} tag first first-dir  ; then
		  echo "PASS: test 12" >>${LOGFILE}
		else
		  echo "FAIL: test 12" | tee -a ${LOGFILE}; exit 1
		fi

		# rtag the empty directory
		if ${CVS} rtag empty first-dir  ; then
		  echo "PASS: test 13" >>${LOGFILE}
		else
		  echo "FAIL: test 13" | tee -a ${LOGFILE}; exit 1
		fi
		;;

	basic1) # first dive - add a files, first singly, then in a group.
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf first-dir
		mkdir ${CVSROOT_DIRNAME}/first-dir
		# check out an empty directory
		if ${CVS} co first-dir  ; then
		  echo "PASS: test 13a" >>${LOGFILE}
		else
		  echo "FAIL: test 13a" | tee -a ${LOGFILE}; exit 1
		fi

		cd first-dir
		files=first-file
		for i in a b ; do
			for j in ${files} ; do
				echo $j > $j
			done

			for do in add rm ; do
				for j in ${do} "commit -m test" ; do
					# ${do}
					if ${CVS} $j ${files}  >> ${LOGFILE} 2>&1; then
					  echo "PASS: test 14-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 14-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update it.
					if [ "${do}" = "rm" -a "$j" != "commit -m test" ] || ${CVS} update ${files} ; then
					  echo "PASS: test 15-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 15-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update all.
					if ${CVS} update  ; then
					  echo "PASS: test 16-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 16-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# status all.
					if ${CVS} status  >> ${LOGFILE}; then
					  echo "PASS: test 17-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 17-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

		# FIXME: this one doesn't work yet for added files.
					# log all.
					if ${CVS} log  >> ${LOGFILE}; then
					  echo "PASS: test 18-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 18-${do}-$j" | tee -a ${LOGFILE}
					fi

					if test "x${do}-$j" = "xadd-add" || test "x${do}-$j" = "xrm-rm" ; then
					  true
					else
					  # diff -c all
					  if ${CVS} diff -c  >> ${LOGFILE} || [ $? = 1 ] ; then
					    echo "PASS: test 19-${do}-$j" >>${LOGFILE}
					  else
					    echo "FAIL: test 19-${do}-$j" | tee -a ${LOGFILE}
					  fi

					  # diff -u all
					  if ${CVS} diff -u  >> ${LOGFILE} || [ $? = 1 ] ; then
					    echo "PASS: test 20-${do}-$j" >>${LOGFILE}
					  else
					    echo "FAIL: test 20-${do}-$j" | tee -a ${LOGFILE}
					  fi
					fi

					cd ..
					# update all.
					if ${CVS} update  ; then
					  echo "PASS: test 21-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 21-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# log all.
		# FIXME: doesn't work right for added files.
					if ${CVS} log first-dir  >> ${LOGFILE}; then
					  echo "PASS: test 22-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 22-${do}-$j" | tee -a ${LOGFILE}
					fi

					# status all.
					if ${CVS} status first-dir  >> ${LOGFILE}; then
					  echo "PASS: test 23-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 23-${do}-$j" | tee -a ${LOGFILE}; exit 1
					fi

					# update all.
					if ${CVS} update first-dir  ; then
					  echo "PASS: test 24-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 24-${do}-$j" | tee -a ${LOGFILE} ; exit 1
					fi

					if test "x${do}-$j" = "xadd-add" || test "x${do}-$j" = "xrm-rm" ; then
					  echo "PASS: test 25-${do}-$j" >>${LOGFILE}
					else
					  # diff all
					  if ${CVS} diff -u  >> ${LOGFILE} || [ $? = 1 ] ; then
					    echo "PASS: test 25-${do}-$j" >>${LOGFILE}
					  else
					    echo "FAIL: test 25-${do}-$j" | tee -a ${LOGFILE}
					    # FIXME; exit 1
					  fi

					  # diff all
					  if ${CVS} diff -u first-dir  >> ${LOGFILE} || [ $? = 1 ] ; then
					    echo "PASS: test 26-${do}-$j" >>${LOGFILE}
					  else
					    echo "FAIL: test 26-${do}-$j" | tee -a ${LOGFILE}
					    # FIXME; exit 1
					  fi
					fi

					# update all.
					if ${CVS} co first-dir  ; then
					  echo "PASS: test 27-${do}-$j" >>${LOGFILE}
					else
					  echo "FAIL: test 27-${do}-$j" | tee -a ${LOGFILE} ; exit 1
					fi

					cd first-dir
				done # j
				rm -f ${files}
			done # do

			files="file2 file3 file4 file5"
		done
		if ${CVS} tag first-dive  ; then
		  echo "PASS: test 28" >>${LOGFILE}
		else
		  echo "FAIL: test 28" | tee -a ${LOGFILE} ; exit 1
		fi
		cd ..
		;;

	basic2) # second dive - add bunch o' files in bunch o' added directories
		for i in first-dir dir1 dir2 dir3 dir4 ; do
			if [ ! -d $i ] ; then
				mkdir $i
				if ${CVS} add $i  >> ${LOGFILE}; then
				  echo "PASS: test 29-$i" >>${LOGFILE}
				else
				  echo "FAIL: test 29-$i" | tee -a ${LOGFILE} ; exit 1
				fi
			fi

			cd $i

			for j in file6 file7 file8 file9 file10 file11 file12 file13; do
				echo $j > $j
			done

			if ${CVS} add file6 file7 file8 file9 file10 file11 file12 file13  2>> ${LOGFILE}; then
				echo "PASS: test 30-$i-$j" >>${LOGFILE}
			else
				echo "FAIL: test 30-$i-$j" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		cd ../../../../..
		if ${CVS} update first-dir  ; then
			echo "PASS: test 31" >>${LOGFILE}
		else
			echo "FAIL: test 31" | tee -a ${LOGFILE} ; exit 1
		fi

		# fixme: doesn't work right for added files.
		if ${CVS} log first-dir  >> ${LOGFILE}; then
			echo "PASS: test 32" >>${LOGFILE}
		else
			echo "FAIL: test 32" | tee -a ${LOGFILE} # ; exit 1
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
			echo "PASS: test 33" >>${LOGFILE}
		else
			echo "FAIL: test 33" | tee -a ${LOGFILE} ; exit 1
		fi

#		if ${CVS} diff -u first-dir   >> ${LOGFILE} || [ $? = 1 ] ; then
#			echo "PASS: test 34" >>${LOGFILE}
#		else
#			echo "FAIL: test 34" | tee -a ${LOGFILE} # ; exit 1
#		fi

		if ${CVS} ci -m "second dive" first-dir  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 35" >>${LOGFILE}
		else
			echo "FAIL: test 35" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} tag second-dive first-dir  ; then
			echo "PASS: test 36" >>${LOGFILE}
		else
			echo "FAIL: test 36" | tee -a ${LOGFILE} ; exit 1
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

			if ${CVS} rm file7 file9 file11 file13  2>> ${LOGFILE}; then
				echo "PASS: test 37-$i" >>${LOGFILE}
			else
				echo "FAIL: test 37-$i" | tee -a ${LOGFILE} ; exit 1
			fi

			# and add some new ones
			for j in file14 file15 file16 file17 ; do
				echo $j > $j
			done

			if ${CVS} add file14 file15 file16 file17  2>> ${LOGFILE}; then
				echo "PASS: test 38-$i" >>${LOGFILE}
			else
				echo "FAIL: test 38-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		cd ../../../../..
		if ${CVS} update first-dir  ; then
			echo "PASS: test 39" >>${LOGFILE}
		else
			echo "FAIL: test 39" | tee -a ${LOGFILE} ; exit 1
		fi

		# fixme: doesn't work right for added files
		if ${CVS} log first-dir  >> ${LOGFILE}; then
			echo "PASS: test 40" >>${LOGFILE}
		else
			echo "FAIL: test 40" | tee -a ${LOGFILE} # ; exit 1
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
			echo "PASS: test 41" >>${LOGFILE}
		else
			echo "FAIL: test 41" | tee -a ${LOGFILE} ; exit 1
		fi

#		if ${CVS} diff -u first-dir  >> ${LOGFILE} || [ $? = 1 ] ; then
#			echo "PASS: test 42" >>${LOGFILE}
#		else
#			echo "FAIL: test 42" | tee -a ${LOGFILE} # ; exit 1
#		fi

		if ${CVS} ci -m "third dive" first-dir  >>${LOGFILE} 2>&1; then
			echo "PASS: test 43" >>${LOGFILE}
		else
			echo "FAIL: test 43" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} tag third-dive first-dir  ; then
			echo "PASS: test 44" >>${LOGFILE}
		else
			echo "FAIL: test 44" | tee -a ${LOGFILE} ; exit 1
		fi

		if echo "yes" | ${CVS} release -d first-dir  ; then
			echo "PASS: test 45" >>${LOGFILE}
		else
			echo "FAIL: test 45" | tee -a ${LOGFILE} ; exit 1
		fi

		# end of third dive
		if [ -d test-dir ] ; then
			echo "FAIL: test 45.5" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 45.5" >>${LOGFILE}
		fi

		;;

	rtags) # now try some rtags
		# rtag HEADS
		if ${CVS} rtag rtagged-by-head first-dir  ; then
			echo "PASS: test 46" >>${LOGFILE}
		else
			echo "FAIL: test 46" | tee -a ${LOGFILE} ; exit 1
		fi

		# tag by tag
		if ${CVS} rtag -r rtagged-by-head rtagged-by-tag first-dir  ; then
			echo "PASS: test 47" >>${LOGFILE}
		else
			echo "FAIL: test 47" | tee -a ${LOGFILE} ; exit 1
		fi

		# tag by revision
		if ${CVS} rtag -r1.1 rtagged-by-revision first-dir  ; then
			echo "PASS: test 48" >>${LOGFILE}
		else
			echo "FAIL: test 48" | tee -a ${LOGFILE} ; exit 1
		fi

		# rdiff by revision
		if ${CVS} rdiff -r1.1 -rrtagged-by-head first-dir  >> ${LOGFILE} || [ $? = 1 ] ; then
			echo "PASS: test 49" >>${LOGFILE}
		else
			echo "FAIL: test 49" | tee -a ${LOGFILE} ; exit 1
		fi

		# now export by rtagged-by-head and rtagged-by-tag and compare.
		rm -rf first-dir
		if ${CVS} export -r rtagged-by-head first-dir  ; then
			echo "PASS: test 50" >>${LOGFILE}
		else
			echo "FAIL: test 50" | tee -a ${LOGFILE} ; exit 1
		fi

		mv first-dir 1dir
		if ${CVS} export -r rtagged-by-tag first-dir  ; then
			echo "PASS: test 51" >>${LOGFILE}
		else
			echo "FAIL: test 51" | tee -a ${LOGFILE} ; exit 1
		fi

		directory_cmp 1dir first-dir

		if $ISDIFF ; then
			echo "FAIL: test 52" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 52" >>${LOGFILE}
		fi
		rm -rf 1dir first-dir

		# checkout by revision vs export by rtagged-by-revision and compare.
		if ${CVS} export -rrtagged-by-revision -d export-dir first-dir  ; then
			echo "PASS: test 53" >>${LOGFILE}
		else
			echo "FAIL: test 53" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} co -r1.1 first-dir  ; then
			echo "PASS: test 54" >>${LOGFILE}
		else
			echo "FAIL: test 54" | tee -a ${LOGFILE} ; exit 1
		fi

		# directory copies are done in an oblique way in order to avoid a bug in sun's tmp filesystem.
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - * | (cd ../first-dir.cpy ; tar xf -))

		directory_cmp first-dir export-dir

		if $ISDIFF ; then 
			echo "FAIL: test 55" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 55" >>${LOGFILE}
		fi

		# interrupt, while we've got a clean 1.1 here, let's import it into another tree.
		cd export-dir
		if ${CVS} import -m "first-import" second-dir first-immigration immigration1 immigration1_0  ; then
			echo "PASS: test 56" >>${LOGFILE}
		else
			echo "FAIL: test 56" | tee -a ${LOGFILE} ; exit 1
		fi
		cd ..

		if ${CVS} export -r HEAD second-dir  ; then
			echo "PASS: test 57" >>${LOGFILE}
		else
			echo "FAIL: test 57" | tee -a ${LOGFILE} ; exit 1
		fi

		directory_cmp first-dir second-dir

		if $ISDIFF ; then
			echo "FAIL: test 58" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 58" >>${LOGFILE}
		fi

		rm -rf export-dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - * | (cd ../first-dir ; tar xf -))

		# update the top, cancelling sticky tags, retag, update other copy, compare.
		cd first-dir
		if ${CVS} update -A -l *file*  2>> ${LOGFILE}; then
			echo "PASS: test 59" >>${LOGFILE}
		else
			echo "FAIL: test 59" | tee -a ${LOGFILE} ; exit 1
		fi

		# If we don't delete the tag first, cvs won't retag it.
		# This would appear to be a feature.
		if ${CVS} tag -l -d rtagged-by-revision  ; then
			echo "PASS: test 60a" >>${LOGFILE}
		else
			echo "FAIL: test 60a" | tee -a ${LOGFILE} ; exit 1
		fi
		if ${CVS} tag -l rtagged-by-revision  ; then
			echo "PASS: test 60b" >>${LOGFILE}
		else
			echo "FAIL: test 60b" | tee -a ${LOGFILE} ; exit 1
		fi

		cd ..
		mv first-dir 1dir
		mv first-dir.cpy first-dir
		cd first-dir

		if ${CVS} diff -u  >> ${LOGFILE} || [ $? = 1 ] ; then
			echo "PASS: test 61" >>${LOGFILE}
		else
			echo "FAIL: test 61" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} update  ; then
			echo "PASS: test 62" >>${LOGFILE}
		else
			echo "FAIL: test 62" | tee -a ${LOGFILE} ; exit 1
		fi

		cd ..

		#### FIXME: is this expected to work???  Need to investigate
		#### and fix or remove the test.
#		directory_cmp 1dir first-dir
#
#		if $ISDIFF ; then
#			echo "FAIL: test 63" | tee -a ${LOGFILE} # ; exit 1
#		else
#			echo "PASS: test 63" >>${LOGFILE}
#		fi
		rm -rf 1dir first-dir

		if ${CVS} his -e -a  >> ${LOGFILE}; then
			echo "PASS: test 64" >>${LOGFILE}
		else
			echo "FAIL: test 64" | tee -a ${LOGFILE} ; exit 1
		fi
		;;

	death) # next dive.  test death support.
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		mkdir  ${CVSROOT_DIRNAME}/first-dir
		if ${CVS} co first-dir  ; then
			echo "PASS: test 65" >>${LOGFILE}
		else
			echo "FAIL: test 65" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir

		# add a file.
		touch file1
		if ${CVS} add file1  2>> ${LOGFILE}; then
			echo "PASS: test 66" >>${LOGFILE}
		else
			echo "FAIL: test 66" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 67" >>${LOGFILE}
		else
			echo "FAIL: test 67" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
			echo "PASS: test 68" >>${LOGFILE}
		else
			echo "FAIL: test 68" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
			echo "PASS: test 69" >>${LOGFILE}
		else
			echo "FAIL: test 69" | tee -a ${LOGFILE} ; exit 1
		fi

		# add again and create second file
		touch file1 file2
		if ${CVS} add file1 file2  2>> ${LOGFILE}; then
			echo "PASS: test 70" >>${LOGFILE}
		else
			echo "FAIL: test 70" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 71" >>${LOGFILE}
		else
			echo "FAIL: test 71" | tee -a ${LOGFILE} ; exit 1
		fi

		# log
		if ${CVS} log file1  >> ${LOGFILE}; then
			echo "PASS: test 72" >>${LOGFILE}
		else
			echo "FAIL: test 72" | tee -a ${LOGFILE} ; exit 1
		fi


		# branch1
		if ${CVS} tag -b branch1  ; then
			echo "PASS: test 73" >>${LOGFILE}
		else
			echo "FAIL: test 73" | tee -a ${LOGFILE} ; exit 1
		fi

		# and move to the branch.
		if ${CVS} update -r branch1  ; then
			echo "PASS: test 74" >>${LOGFILE}
		else
			echo "FAIL: test 74" | tee -a ${LOGFILE} ; exit 1
		fi

		# add a file in the branch
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
			echo "PASS: test 75" >>${LOGFILE}
		else
			echo "FAIL: test 75" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 76" >>${LOGFILE}
		else
			echo "FAIL: test 76" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove
		rm file3
		if ${CVS} rm file3  2>> ${LOGFILE}; then
			echo "PASS: test 77" >>${LOGFILE}
		else
			echo "FAIL: test 77" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
			echo "PASS: test 78" >>${LOGFILE}
		else
			echo "FAIL: test 78" | tee -a ${LOGFILE} ; exit 1
		fi

		# add again
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
			echo "PASS: test 79" >>${LOGFILE}
		else
			echo "FAIL: test 79" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 80" >>${LOGFILE}
		else
			echo "FAIL: test 80" | tee -a ${LOGFILE} ; exit 1
		fi

		# change the first file
		echo line2 from branch1 >> file1

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 81" >>${LOGFILE}
		else
			echo "FAIL: test 81" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove the second
		rm file2
		if ${CVS} rm file2  2>> ${LOGFILE}; then
			echo "PASS: test 82" >>${LOGFILE}
		else
			echo "FAIL: test 82" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
			echo "PASS: test 83" >>${LOGFILE}
		else
			echo "FAIL: test 83" | tee -a ${LOGFILE} ; exit 1
		fi

		# back to the trunk.
		if ${CVS} update -A  2>> ${LOGFILE}; then
			echo "PASS: test 84" >>${LOGFILE}
		else
			echo "FAIL: test 84" | tee -a ${LOGFILE} ; exit 1
		fi

		if [ -f file3 ] ; then
			echo "FAIL: test 85" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 85" >>${LOGFILE}
		fi

		# join
		if ${CVS} update -j branch1  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 86" >>${LOGFILE}
		else
			echo "FAIL: test 86" | tee -a ${LOGFILE} ; exit 1
		fi

		if [ -f file3 ] ; then
			echo "PASS: test 87" >>${LOGFILE}
		else
			echo "FAIL: test 87" | tee -a ${LOGFILE} ; exit 1
		fi

		# Make sure that we joined the correct change to file1
		if echo line2 from branch1 | cmp - file1 >/dev/null; then
			echo 'PASS: test 87a' >>${LOGFILE}
		else
			echo 'FAIL: test 87a' | tee -a ${LOGFILE}
			exit 1
		fi

		# update
		if ${CVS} update  ; then
			echo "PASS: test 88" >>${LOGFILE}
		else
			echo "FAIL: test 88" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} 2>&1; then
			echo "PASS: test 89" >>${LOGFILE}
		else
			echo "FAIL: test 89" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove first file.
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
			echo "PASS: test 90" >>${LOGFILE}
		else
			echo "FAIL: test 90" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
			echo "PASS: test 91" >>${LOGFILE}
		else
			echo "FAIL: test 91" | tee -a ${LOGFILE} ; exit 1
		fi

		if [ -f file1 ] ; then
			echo "FAIL: test 92" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 92" >>${LOGFILE}
		fi

		# back to branch1
		if ${CVS} update -r branch1  2>> ${LOGFILE}; then
			echo "PASS: test 93" >>${LOGFILE}
		else
			echo "FAIL: test 93" | tee -a ${LOGFILE} ; exit 1
		fi

		if [ -f file1 ] ; then
			echo "PASS: test 94" >>${LOGFILE}
		else
			echo "FAIL: test 94" | tee -a ${LOGFILE} ; exit 1
		fi

		# and join
		if ${CVS} update -j HEAD  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 95" >>${LOGFILE}
		else
			echo "FAIL: test 95" | tee -a ${LOGFILE} ; exit 1
		fi

		cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		;;

	import) # test death after import
		# import
		mkdir import-dir ; cd import-dir

		for i in 1 2 3 4 ; do
			echo imported file"$i" > imported-file"$i"
		done

		if ${CVS} import -m first-import first-dir vendor-branch junk-1_0  ; then
			echo "PASS: test 96" >>${LOGFILE}
		else
			echo "FAIL: test 96" | tee -a ${LOGFILE} ; exit 1
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
			echo "PASS: test 97" >>${LOGFILE}
		else
			echo "FAIL: test 97" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir
		for i in 1 2 3 4 ; do
			if [ -f imported-file"$i" ] ; then
				echo "PASS: test 98-$i" >>${LOGFILE}
			else
				echo "FAIL: test 98-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done

		# remove
		rm imported-file1
		if ${CVS} rm imported-file1  2>> ${LOGFILE}; then
			echo "PASS: test 99" >>${LOGFILE}
		else
			echo "FAIL: test 99" | tee -a ${LOGFILE} ; exit 1
		fi

		# change
		# this sleep is significant.  Otherwise, on some machines, things happen so
		# fast that the file mod times do not differ.
		sleep 1
		echo local-change >> imported-file2

		# commit
		if ${CVS} ci -m local-changes  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 100" >>${LOGFILE}
		else
			echo "FAIL: test 100" | tee -a ${LOGFILE} ; exit 1
		fi

		# log
		if ${CVS} log imported-file1 | grep '1.1.1.2 (dead)'  ; then
			echo "FAIL: test 101" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 101" >>${LOGFILE}
		fi

		# update into the vendor branch.
		if ${CVS} update -rvendor-branch  ; then
			echo "PASS: test 102" >>${LOGFILE}
		else
			echo "FAIL: test 102" | tee -a ${LOGFILE} ; exit 1
		fi

		# remove file4 on the vendor branch
		rm imported-file4

		if ${CVS} rm imported-file4  2>> ${LOGFILE}; then
			echo "PASS: test 103" >>${LOGFILE}
		else
			echo "FAIL: test 103" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m vendor-removed imported-file4 >>${LOGFILE}; then
			echo "PASS: test 104" >>${LOGFILE}
		else
			echo "FAIL: test 104" | tee -a ${LOGFILE} ; exit 1
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
			echo "PASS: test 105" >>${LOGFILE}
		else
			echo "FAIL: test 105" | tee -a ${LOGFILE} ; exit 1
		fi

		# second import - file4 deliberately unchanged
		cd ../import-dir
		for i in 1 2 3 ; do
			echo rev 2 of file $i >> imported-file"$i"
		done

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0  ; then
			echo "PASS: test 106" >>${LOGFILE}
		else
			echo "FAIL: test 106" | tee -a ${LOGFILE} ; exit 1
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
			echo "PASS: test 107" >>${LOGFILE}
		else
			echo "FAIL: test 107" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir

		if [ -f imported-file1 ] ; then
			echo "FAIL: test 108" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 108" >>${LOGFILE}
		fi

		for i in 2 3 ; do
			if [ -f imported-file"$i" ] ; then
				echo "PASS: test 109-$i" >>${LOGFILE}
			else
				echo "FAIL: test 109-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done

		# check vendor branch for file4
		if ${CVS} update -rvendor-branch  ; then
			echo "PASS: test 110" >>${LOGFILE}
		else
			echo "FAIL: test 110" | tee -a ${LOGFILE} ; exit 1
		fi

		if [ -f imported-file4 ] ; then
			echo "PASS: test 111" >>${LOGFILE}
		else
			echo "FAIL: test 111" | tee -a ${LOGFILE} ; exit 1
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
			echo "PASS: test 112" >>${LOGFILE}
		else
			echo "FAIL: test 112" | tee -a ${LOGFILE} ; exit 1
		fi

		cd ..

		if ${CVS} co -jjunk-1_0 -jjunk-2_0 first-dir  >>${LOGFILE} 2>&1; then
			echo "PASS: test 113" >>${LOGFILE}
		else
			echo "FAIL: test 113" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir

		if [ -f imported-file1 ] ; then
			echo "FAIL: test 114" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 114" >>${LOGFILE}
		fi

		for i in 2 3 ; do
			if [ -f imported-file"$i" ] ; then
				echo "PASS: test 115-$i" >>${LOGFILE}
			else
				echo "FAIL: test 115-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done

		if cat imported-file2 | grep '===='  >> ${LOGFILE}; then
			echo "PASS: test 116" >>${LOGFILE}
		else
			echo "FAIL: test 116" | tee -a ${LOGFILE} ; exit 1
		fi
		cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		;;

	new) # look for stray "no longer pertinent" messages.
		rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		mkdir ${CVSROOT_DIRNAME}/first-dir

		if ${CVS} co first-dir  ; then
			echo "PASS: test 117" >>${LOGFILE}
		else
			echo "FAIL: test 117" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir
		touch a

		if ${CVS} add a  2>>${LOGFILE}; then
			echo "PASS: test 118" >>${LOGFILE}
		else
			echo "FAIL: test 118" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} ci -m added  >>${LOGFILE} 2>&1; then
			echo "PASS: test 119" >>${LOGFILE}
		else
			echo "FAIL: test 119" | tee -a ${LOGFILE} ; exit 1
		fi

		rm a

		if ${CVS} rm a  2>>${LOGFILE}; then
			echo "PASS: test 120" >>${LOGFILE}
		else
			echo "FAIL: test 120" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} ci -m removed >>${LOGFILE} ; then
			echo "PASS: test 121" >>${LOGFILE}
		else
			echo "FAIL: test 121" | tee -a ${LOGFILE} ; exit 1
		fi

		if ${CVS} update -A  2>&1 | grep longer ; then
			echo "FAIL: test 122" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 122" >>${LOGFILE}
		fi

		if ${CVS} update -rHEAD 2>&1 | grep longer ; then
			echo "FAIL: test 123" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 123" >>${LOGFILE}
		fi

		cd .. ; rm -rf first-dir ; rm -rf ${CVSROOT_DIRNAME}/first-dir
		;;

	conflicts)
		rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		mkdir ${CVSROOT_DIRNAME}/first-dir

		mkdir 1
		cd 1

		if ${CVS} co first-dir ; then
			echo 'PASS: test 124' >>${LOGFILE}
		else
			echo 'FAIL: test 124' | tee -a ${LOGFILE}
		fi

		cd first-dir
		touch a

		if ${CVS} add a 2>>${LOGFILE} ; then
			echo 'PASS: test 125' >>${LOGFILE}
		else
			echo 'FAIL: test 125' | tee -a ${LOGFILE}
		fi

		if ${CVS} ci -m added >>${LOGFILE} 2>&1; then
			echo 'PASS: test 126' >>${LOGFILE}
		else
			echo 'FAIL: test 126' | tee -a ${LOGFILE}
		fi

		cd ../..
		mkdir 2
		cd 2

		if ${CVS} co first-dir ; then
			echo 'PASS: test 127' >>${LOGFILE}
		else
			echo 'FAIL: test 127' | tee -a ${LOGFILE}
		fi
		cd first-dir
		if test -f a; then
			echo 'PASS: test 127a' >>${LOGFILE}
		else
			echo 'FAIL: test 127a' | tee -a ${LOGFILE}
		fi

		cd ../../1/first-dir
		echo add a line >>a
		if ${CVS} ci -m changed >>${LOGFILE} 2>&1; then
			echo 'PASS: test 128' >>${LOGFILE}
		else
			echo 'FAIL: test 128' | tee -a ${LOGFILE}
		fi

		cd ../../2/first-dir
		echo add a conflicting line >>a
		if ${CVS} ci -m changed >>${LOGFILE} 2>&1; then
			echo 'FAIL: test 129' | tee -a ${LOGFILE}
		else
			# Should be printing `out of date check failed'.
			echo 'PASS: test 129' >>${LOGFILE}
		fi

		if ${CVS} update 2>>${LOGFILE}; then
			# We should get a conflict, but that doesn't affect
			# exit status
			echo 'PASS: test 130' >>${LOGFILE}
		else
			echo 'FAIL: test 130' | tee -a ${LOGFILE}
		fi

		# Try to check in the file with the conflict markers in it.
		if ${CVS} ci -m try 2>>${LOGFILE}; then
			echo 'FAIL: test 131' | tee -a ${LOGFILE}
		else
			# Should tell us to resolve conflict first
			echo 'PASS: test 131' >>${LOGFILE}
		fi

		echo lame attempt at resolving it >>a
		# Try to check in the file with the conflict markers in it.
		if ${CVS} ci -m try >>${LOGFILE} 2>&1; then
			echo 'FAIL: test 132' | tee -a ${LOGFILE}
		else
			# Should tell us to resolve conflict first
			echo 'PASS: test 132' >>${LOGFILE}
		fi

		echo resolve conflict >a
		if ${CVS} ci -m resolved >>${LOGFILE} 2>&1; then
			echo 'PASS: test 133' >>${LOGFILE}
		else
			echo 'FAIL: test 133' | tee -a ${LOGFILE}
		fi

		# Now test that we can add a file in one working directory
		# and have an update in another get it.
		cd ../../1/first-dir
		echo abc >abc
		if ${testcvs} add abc >>${LOGFILE} 2>&1; then
			echo 'PASS: test 134' >>${LOGFILE}
		else
			echo 'FAIL: test 134' | tee -a ${LOGFILE}
		fi
		if ${testcvs} ci -m 'add abc' abc >>${LOGFILE} 2>&1; then
			echo 'PASS: test 135' >>${LOGFILE}
		else
			echo 'FAIL: test 135' | tee -a ${LOGFILE}
		fi
		cd ../../2
		if ${testcvs} -q update >>${LOGFILE}; then
			echo 'PASS: test 136' >>${LOGFILE}
		else
			echo 'FAIL: test 136' | tee -a ${LOGFILE}
		fi
		if test -f first-dir/abc; then
			echo 'PASS: test 137' >>${LOGFILE}
		else
			echo 'FAIL: test 137' | tee -a ${LOGFILE}
		fi

		# Now test something similar, but in which the parent directory
		# (not the directory in question) has the Entries.Static flag
		# set.
		cd ../1/first-dir
		mkdir subdir
		if ${testcvs} add subdir >>${LOGFILE}; then
			echo 'PASS: test 138' >>${LOGFILE}
		else
			echo 'FAIL: test 138' | tee -a ${LOGFILE}
		fi
		cd ../..
		mkdir 3
		cd 3
		if ${testcvs} -q co first-dir/abc first-dir/subdir \
		    >>${LOGFILE}; then
		  echo 'PASS: test 139' >>${LOGFILE}
		else
		  echo 'FAIL: test 139' | tee -a ${LOGFILE}
		fi
		cd ../1/first-dir/subdir
		echo sss >sss
		if ${testcvs} add sss >>${LOGFILE} 2>&1; then
		  echo 'PASS: test 140' >>${LOGFILE}
		else
		  echo 'FAIL: test 140' | tee -a ${LOGFILE}
		fi
		if ${testcvs} ci -m adding sss >>${LOGFILE} 2>&1; then
		  echo 'PASS: test 140' >>${LOGFILE}
		else
		  echo 'FAIL: test 140' | tee -a ${LOGFILE}
		fi
		cd ../../../3/first-dir
		if ${testcvs} -q update >>${LOGFILE}; then
		  echo 'PASS: test 141' >>${LOGFILE}
		else
		  echo 'FAIL: test 141' | tee -a ${LOGFILE}
		fi
		if test -f subdir/sss; then
		  echo 'PASS: test 142' >>${LOGFILE}
		else
		  echo 'FAIL: test 142' | tee -a ${LOGFILE}
		fi

		cd ../.. 
		rm -rf 1 2 3 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
		;;
	modules)
	  # The following line stolen from cvsinit.sh.  FIXME: create our
	  # repository via cvsinit.sh; that way we test it too.
	  (cd ${CVSROOT_DIRNAME}/CVSROOT; ci -q -u -t/dev/null \
	    -m'initial checkin of modules' modules)

	  rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  mkdir 1
	  cd 1

	  if ${testcvs} -q co first-dir; then
	    echo 'PASS: test 143' >>${LOGFILE}
	  else
	    echo 'FAIL: test 143' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd first-dir
	  mkdir subdir
	  ${testcvs} add subdir >>${LOGFILE}
	  cd subdir

	  touch a

	  if ${testcvs} add a 2>>${LOGFILE} ; then
	    echo 'PASS: test 144' >>${LOGFILE}
	  else
	    echo 'FAIL: test 144' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 145' >>${LOGFILE}
	  else
	    echo 'FAIL: test 145' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd ..
	  if ${testcvs} -q co CVSROOT >>${LOGFILE}; then
	    echo 'PASS: test 146' >>${LOGFILE}
	  else
	    echo 'FAIL: test 146' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  # Here we test that CVS can deal with CVSROOT (whose repository
	  # is at top level) in the same directory as subdir (whose repository
	  # is a subdirectory of first-dir).  TODO: Might want to check that
	  # files can actually get updated in this state.
	  if ${testcvs} -q update; then
	    echo 'PASS: test 147' >>${LOGFILE}
	  else
	    echo 'FAIL: test 147' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  echo realmodule first-dir/subdir a >>CVSROOT/modules
	  echo aliasmodule -a first-dir/subdir/a >>CVSROOT/modules
	  if ${testcvs} ci -m 'add modules' CVSROOT/modules \
	      >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 148' >>${LOGFILE}
	  else
	    echo 'FAIL: test 148' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ..
	  if ${testcvs} co realmodule >>${LOGFILE}; then
	    echo 'PASS: test 149' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d realmodule && test -f realmodule/a; then
	    echo 'PASS: test 150' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} co aliasmodule >>${LOGFILE}; then
	    echo 'PASS: test 151' >>${LOGFILE}
	  else
	    echo 'FAIL: test 151' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d aliasmodule; then
	    echo 'FAIL: test 152' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 152' >>${LOGFILE}
	  fi
	  echo abc >>first-dir/subdir/a
	  if (${testcvs} -q co aliasmodule | tee test153.tmp) \
	      >>${LOGFILE}; then
	    echo 'PASS: test 153' >>${LOGFILE}
	  else
	    echo 'FAIL: test 153' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  echo 'M first-dir/subdir/a' >ans153.tmp
	  if cmp test153.tmp ans153.tmp; then
	    echo 'PASS: test 154' >>${LOGFILE}
	  else
	    echo 'FAIL: test 154' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} -q co realmodule; then
	    echo 'PASS: test 155' >>${LOGFILE}
	  else
	    echo 'FAIL: test 155' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ..
	  rm -rf 1 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;
	mflag)
	  for message in '' ' ' '	
           ' '    	  	test' ; do
	    # Set up
	    mkdir a-dir; cd a-dir
	    # Test handling of -m during import
	    echo testa >>test
	    if ${testcvs} import -m "$message" a-dir A A1 >>${LOGFILE} 2>&1;then
	      echo 'PASS: test 156' >>${LOGFILE}
	    else
	      echo 'FAIL: test 156' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Must import twice since the first time uses inline code that
	    # avoids RCS call.
	    echo testb >>test
	    if ${testcvs} import -m "$message" a-dir A A2 >>${LOGFILE} 2>&1;then
	      echo 'PASS: test 157' >>${LOGFILE}
	    else
	      echo 'FAIL: test 157' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Test handling of -m during ci
	    cd ..; rm -rf a-dir;
	    if ${testcvs} co a-dir >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 158' >>${LOGFILE}
	    else
	      echo 'FAIL: test 158' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    cd a-dir
	    echo testc >>test
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 159' >>${LOGFILE}
	    else
	      echo 'FAIL: test 159' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Test handling of -m during rm/ci
	    rm test;
	    if ${testcvs} rm test >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 160' >>${LOGFILE}
	    else
	      echo 'FAIL: test 160' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
	      echo 'PASS: test 161' >>${LOGFILE}
	    else
	      echo 'FAIL: test 161' | tee -a ${LOGFILE}
	      exit 1
	    fi
	    # Clean up
	    cd ..; rm -rf a-dir ${CVSROOT_DIRNAME}/a-dir
	  done
	  ;;
	errmsg1)
	  mkdir ${CVSROOT_DIRNAME}/1dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co 1dir; then
	    echo 'PASS: test 162' >>${LOGFILE}
	  else
	    echo 'FAIL: test 162' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd 1dir
	  touch foo
	  if ${testcvs} add foo 2>>${LOGFILE}; then
	    echo 'PASS: test 163' >>${LOGFILE}
	  else
	    echo 'FAIL: test 163' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 164' >>${LOGFILE}
	  else
	    echo 'FAIL: test 164' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ../..
	  mkdir 2
	  cd 2
	  if ${testcvs} -q co 1dir >>${LOGFILE}; then
	    echo 'PASS: test 165' >>${LOGFILE}
	  else
	    echo 'FAIL: test 165' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  chmod a-w 1dir
	  cd ../1/1dir
	  rm foo; 
	  if ${testcvs} rm foo >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 166' >>${LOGFILE}
	  else
	    echo 'FAIL: test 166' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} ci -m removed >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 167' >>${LOGFILE}
	  else
	    echo 'FAIL: test 167' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ../../2/1dir
	  ${testcvs} -q update 2>../tst167.err
	  CVSBASE=`basename $testcvs`	# Get basename of CVS executable.
	  cat <<EOF >../tst167.ans
$CVSBASE server: warning: foo is not (any longer) pertinent
$CVSBASE update: unable to remove ./foo: Permission denied
EOF
	  if cmp ../tst167.ans ../tst167.err >/dev/null ||
	  ( echo "$CVSBASE [update aborted]: cannot rename file foo to CVS/,,foo: Permission denied" | cmp - ../tst167.err >/dev/null )
	  then
	    echo 'PASS: test 168' >>${LOGFILE}
	  else
	    echo 'FAIL: test 168' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd ..
	  chmod u+w 1dir
	  cd ..
	  rm -rf 1 2 ${CVSROOT_DIRNAME}/1dir
	  ;;

	*)
	   echo $what is not the name of a test -- ignored
	   ;;
	esac
done

echo "OK, all tests completed."

# TODO:
# * Test `cvs admin'.
# * Test `cvs update -d foo' (where foo does not exist).
# * Test `cvs update foo bar' (where foo and bar are both from the same
#   repository).  Suppose one is a branch--make sure that both directories
#   get updated with the respective correct thing.
# * Zero length files (check in, check out).
# * `cvs update ../foo'.  Also ../../foo ./../foo foo/../../bar /foo/bar
#   foo/.././../bar foo/../bar etc.
# * Test all flags in modules file.
#   Test that ciprog gets run both on checkin in that directory, or a
#     higher-level checkin which recurses into it.
# * Test that $ followed by "Header" followed by $ gets expanded on checkin.
# * Test operations on a directory that contains other directories but has 
#   no files of its own.
# * -t global option
# * cvs rm followed by cvs add or vice versa (with no checkin in between).
# * cvs rm twice (should be a nice error message).
# * -P option to checkout--(a) refrains from checking out new empty dirs,
#   (b) prunes empty dirs already there.
# * Test that cvs -d `hostname`:/tmp/cvs-sanity/non/existent co foo
#   gives an appropriate error (e.g. 
#     Cannot access /tmp/cvs-sanity/non-existent/CVSROOT
#     No such file or directory).
# End of TODO list.

# Remove the test directory, but first change out of it.
cd /tmp
rm -rf ${TESTDIR}

# end of sanity.sh
