#! /bin/sh
:
#	sanity.sh -- a growing testsuite for cvs.
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

# The --keep option will eventually cause all the tests to leave around the
# contents of the /tmp directory; right now only some implement it.  Not
# useful if you are running more than one test.
# FIXME: need some real option parsing so this doesn't depend on the order
# in which they are specified.
if test x"$1" = x"--keep"; then
  shift
  keep=yes
else
  keep=no
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

# Regexp to match what CVS will call itself in output that it prints.
# FIXME: we don't properly quote this--if the name contains . we'll
# just spuriously match a few things; if the name contains other regexp
# special characters we are probably in big trouble.
PROG=`basename ${testcvs}`

# FIXME: try things (what things? checkins?) without -m.
#
# Some of these tests are written to expect -Q.  But testing with
# -Q is kind of bogus, it is not the way users actually use CVS (usually).
# So new tests probably should invoke ${testcvs} directly, rather than ${CVS}.
# and then they've obviously got to do something with the output....
#
CVS="${testcvs} -Q"

LOGFILE=`pwd`/check.log

# Save the previous log in case the person running the tests decides
# they want to look at it.  The extension ".plog" is chosen for consistency
# with dejagnu.
if test -f check.log; then
	mv check.log check.plog
fi

# That we should have to do this is total bogosity, but GNU expr
# version 1.9.4 uses the emacs definition of "$" instead of the unix
# (e.g. SunOS 4.1.3 expr) one.  Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
ENDANCHOR="$"
if expr 'abc
def' : 'abc$' >/dev/null; then
  ENDANCHOR='\'\'
fi

# Work around another GNU expr (version 1.10) bug/incompatibility.
# "." doesn't appear to match a newline (it does with SunOS 4.1.3 expr).
# Note that the workaround is not a complete equivalent of .* because
# the first parenthesized expression in the regexp must match something
# in order for expr to return a successful exit status.
# Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
DOTSTAR='.*'
if expr 'abc
def' : "a${DOTSTAR}f" >/dev/null; then
  : good, it works
else
  DOTSTAR='\(.\|
\)*'
fi

# Work around yet another GNU expr (version 1.10) bug/incompatibility.
# "+" is a special character, yet for unix expr (e.g. SunOS 4.1.3)
# it is not.  I doubt that POSIX allows us to use \+ and assume it means
# (non-special) +, so here is another workaround
# Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
PLUS='+'
if expr 'a +b' : "a ${PLUS}b" >/dev/null; then
  : good, it works
else
  PLUS='\+'
fi

# Likewise, for ?
QUESTION='?'
if expr 'a?b' : "a${QUESTION}b" >/dev/null; then
  : good, it works
else
  QUESTION='\?'
fi

# Cause NextStep 3.3 users to lose in a more graceful fashion.
if expr 'abc
def' : 'abc
def' >/dev/null; then
  : good, it works
else
  echo 'Running these tests requires an "expr" program that can handle'
  echo 'multi-line patterns.  Make sure that such an expr (GNU, or many but'
  echo 'not all vendor-supplied versions) is in your path.'
  exit 1
fi

# Warn SunOS, SysVr3.2, etc., users that they may be partially losing
if expr 'a
b' : 'a
c' >/dev/null; then
  echo 'Warning: you are using a version of expr which does not correctly'
  echo 'match multi-line patterns.  Some tests may spuriously pass.'
  echo 'You may wish to make sure GNU expr is in your path.'
else
  : good, it works
fi

pass ()
{
  echo "PASS: $1" >>${LOGFILE}
}

fail ()
{
  echo "FAIL: $1" | tee -a ${LOGFILE}
  # This way the tester can go and see what remnants were left
  exit 1
}

# See dotest and dotest_fail for explanation (this is the parts
# of the implementation common to the two).
dotest_internal ()
{
  # expr can't distinguish between "zero characters matched" and "no match",
  # so special-case it.
  if test -z "$3"; then
    if test -s ${TESTDIR}/dotest.tmp; then
      echo "** expected: " >>${LOGFILE}
      echo "$3" >>${LOGFILE}
      echo "** got: " >>${LOGFILE}
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      fail "$1"
    else
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      pass "$1"
    fi
  else
    if expr "`cat ${TESTDIR}/dotest.tmp`" : \
	"$3"${ENDANCHOR} >/dev/null; then
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      pass "$1"
    else
      if test x"$4" != x; then
	if expr "`cat ${TESTDIR}/dotest.tmp`" : \
	    "$4"${ENDANCHOR} >/dev/null; then
	  cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	  pass "$1"
	else
	  echo "** expected: " >>${LOGFILE}
	  echo "$3" >>${LOGFILE}
	  echo "** or: " >>${LOGFILE}
	  echo "$4" >>${LOGFILE}
	  echo "** got: " >>${LOGFILE}
	  cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	  fail "$1"
	fi
      else
	echo "** expected: " >>${LOGFILE}
	echo "$3" >>${LOGFILE}
	echo "** got: " >>${LOGFILE}
	cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	fail "$1"
      fi
    fi
  fi
}

# Usage:
#  dotest TESTNAME COMMAND OUTPUT [OUTPUT2]
# TESTNAME is the name used in the log to identify the test.
# COMMAND is the command to run; for the test to pass, it exits with 
# exitstatus zero.
# OUTPUT is a regexp which is compared against the output (stdout and
# stderr combined) from the test.  It is anchored to the start and end
# of the output, so should start or end with ".*" if that is what is desired.
# Trailing newlines are stripped from the command's actual output before
# matching against OUTPUT.
# If OUTPUT2 is specified and the output matches it, then it is also
# a pass (partial workaround for the fact that some versions of expr
# lack \|).
dotest ()
{
  if $2 >${TESTDIR}/dotest.tmp 2>&1; then
    : so far so good
  else
    status=$?
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  dotest_internal "$@"
}

# Like dotest except exitstatus should be nonzero.
dotest_fail ()
{
  if $2 >${TESTDIR}/dotest.tmp 2>&1; then
    status=$?
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  else
    : so far so good
  fi
  dotest_internal "$@"
}

# Like dotest except second argument is the required exitstatus.
dotest_status ()
{
  $3 >${TESTDIR}/dotest.tmp 2>&1
  status=$?
  if test "$status" = "$2"; then
    : so far so good
  else
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status; expected $2" >>${LOGFILE}
    fail "$1"
  fi
  dotest_internal "$1" "$3" "$4" "$5"
}

# clean any old remnants
rm -rf ${TESTDIR}
mkdir ${TESTDIR}
cd ${TESTDIR}

# Avoid picking up any stray .cvsrc, etc., from the user running the tests
mkdir home
HOME=${TESTDIR}/home; export HOME

# Remaining arguments are the names of tests to run.
#
# The testsuite is broken up into (hopefully manageably-sized)
# independently runnable tests, so that one can quickly get a result
# from a cvs or testsuite change, and to facilitate understanding the
# tests.

if test x"$*" = x; then
	tests="basica basic1 deep basic2 death branches import new conflicts modules mflag errmsg1 devcom ignore binfiles info"
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
# (now that mkmodules is gone, this doesn't test -i the way it
# used to.  In fact, it looks like a noop to me).
echo "CVSROOT		CVSROOT" > cvsroot/CVSROOT/modules
# The following line stolen from cvsinit.sh.  FIXME: create our
# repository via cvsinit.sh; that way we test it too.
(cd cvsroot/CVSROOT; ci -q -u -t/dev/null \
  -m'initial checkin of modules' modules)

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
dotest_fail 4.75 "test -d tmp" ''

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

# start keeping history
touch ${CVSROOT_DIRNAME}/CVSROOT/history

### The big loop
for what in $tests; do
	case $what in
	basica)
	  # Similar in spirit to some of the basic1, and basic2
	  # tests, but hopefully a lot faster.  Also tests operating on
	  # files two directories down *without* operating on the parent dirs.

	  # Using mkdir in the repository is used throughout these
	  # tests to create a top-level directory.  I think instead it
	  # should be:
	  #   cvs co -l .
	  #   mkdir first-dir
	  #   cvs add first-dir
	  # but currently that works only for local CVS, not remote.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest basica-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # Test a few operations, to ensure they gracefully do
	  # nothing in an empty directory.
	  dotest basica-1a0 "${testcvs} -q update" ''
	  dotest basica-1a1 "${testcvs} -q diff -c" ''
	  dotest basica-1a2 "${testcvs} -q status" ''

	  mkdir sdir
	  dotest basica-2 "${testcvs} add sdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir added to the repository'
	  cd sdir
	  mkdir ssdir
	  dotest basica-3 "${testcvs} add ssdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir added to the repository'
	  cd ssdir
	  echo ssfile >ssfile

	  # Trying to commit it without a "cvs add" should be an error.
	  # The "use `cvs add' to create an entry" message is the one
	  # that I consider to be more correct, but local cvs prints the
	  # "nothing known" message and noone has gotten around to fixing it.
	  dotest_fail basica-notadded "${testcvs} -q ci ssfile" \
"${PROG} [a-z]*: use "'`cvs add'\'' to create an entry for ssfile
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!' \
"${PROG}"' [a-z]*: nothing known about `ssfile'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'

	  dotest basica-4 "${testcvs} add ssfile" \
"${PROG}"' [a-z]*: scheduling file `ssfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest_fail basica-4a "${testcvs} tag tag0 ssfile" \
"${PROG} [a-z]*: nothing known about ssfile
${PROG} "'\[[a-z]* aborted\]: correct the above errors first!'
	  cd ../..
	  dotest basica-5 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v
done
Checking in sdir/ssdir/ssfile;
/tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
initial revision: 1.1
done'
	  dotest_fail basica-5a \
	    "${testcvs} -q tag BASE sdir/ssdir/ssfile" \
"${PROG} [a-z]*: Attempt to add reserved tag name BASE
${PROG} \[[a-z]* aborted\]: failed to set tag BASE to revision 1.1 in /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v"
	  dotest basica-5b "${testcvs} -q tag NOT_RESERVED" \
'T sdir/ssdir/ssfile'

	  dotest basica-6 "${testcvs} -q update" ''
	  echo "ssfile line 2" >>sdir/ssdir/ssfile
	  dotest_status basica-6.2 1 "${testcvs} -q diff -c" \
'Index: sdir/ssdir/ssfile
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* ssfile	[0-9/]* [0-9:]*	1\.1
--- ssfile	[0-9/]* [0-9:]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
'"${PLUS} ssfile line 2"
	  dotest basica-7 "${testcvs} -q ci -m modify-it" \
'Checking in sdir/ssdir/ssfile;
/tmp/cvs-sanity/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 1.2; previous revision: 1.1
done'
	  dotest_fail basica-nonexist "${testcvs} -q ci nonexist" \
"${PROG}"' [a-z]*: nothing known about `nonexist'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
	  dotest basica-8 "${testcvs} -q update" ''
	  dotest_fail basica-9 \
	    "${testcvs} -q -d /tmp/cvs-sanity/nonexist update" \
"${PROG}: .*/tmp/cvs-sanity/cvsroot value for CVS Root found in CVS/Root
${PROG}"': does not match command line -d /tmp/cvs-sanity/nonexist setting
'"${PROG}"': you may wish to try the cvs command again without the -d option '

	  dotest basica-10 "${testcvs} annotate" \
'Annotations for sdir/ssdir/ssfile
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1.1          .[a-z@][a-z@ ]* [0-9a-zA-Z-]*.: ssfile
1.2          .[a-z@][a-z@ ]* [0-9a-zA-Z-]*.: ssfile line 2'
	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	basic1) # first dive - add a files, first singly, then in a group.
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
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf first-dir
		;;

	deep)
	  # Test the ability to operate on directories nested rather deeply.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest deep-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  for i in dir1 dir2 dir3 dir4 dir5 dir6 dir7 dir8; do
	    mkdir $i
	    dotest deep-2-$i "${testcvs} add $i" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/dir1[/dir0-9]* added to the repository'
	    cd $i
	    echo file1 >file1
	    dotest deep-3-$i "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  done
	  cd ../../../../../../../../..
	  dotest deep-4 "${testcvs} -q ci -m add-them first-dir" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/file1,v
done
Checking in first-dir/dir1/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/file1,v
done
Checking in first-dir/dir1/dir2/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/file1,v
done
Checking in first-dir/dir1/dir2/dir3/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1;
/tmp/cvs-sanity/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
initial revision: 1.1
done'

	  if echo "yes" | ${testcvs} release -d first-dir >>${LOGFILE}; then
	    pass deep-5
	  else
	    fail deep-5
	  fi
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	basic2)
		# Test rtag, import, history, various miscellaneous operations

		# First empty the history file
		rm ${CVSROOT_DIRNAME}/CVSROOT/history
		touch ${CVSROOT_DIRNAME}/CVSROOT/history

		mkdir ${CVSROOT_DIRNAME}/first-dir
		dotest basic2-1 "${testcvs} -q co first-dir" ''
		for i in first-dir dir1 dir2 ; do
			if [ ! -d $i ] ; then
				mkdir $i
				if ${CVS} add $i  >> ${LOGFILE}; then
				  echo "PASS: test 29-$i" >>${LOGFILE}
				else
				  echo "FAIL: test 29-$i" | tee -a ${LOGFILE} ; exit 1
				fi
			fi

			cd $i

			for j in file6 file7; do
				echo $j > $j
			done

			if ${CVS} add file6 file7  2>> ${LOGFILE}; then
				echo "PASS: test 30-$i-$j" >>${LOGFILE}
			else
				echo "FAIL: test 30-$i-$j" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		cd ../../..
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

		# third dive - in bunch o' directories, add bunch o' files,
		# delete some, change some.

		for i in first-dir dir1 dir2 ; do
			cd $i

			# modify a file
			echo file6 >>file6

			# delete a file
			rm file7

			if ${CVS} rm file7  2>> ${LOGFILE}; then
				echo "PASS: test 37-$i" >>${LOGFILE}
			else
				echo "FAIL: test 37-$i" | tee -a ${LOGFILE} ; exit 1
			fi

			# and add a new file
			echo file14 >file14

			if ${CVS} add file14  2>> ${LOGFILE}; then
				echo "PASS: test 38-$i" >>${LOGFILE}
			else
				echo "FAIL: test 38-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done
		cd ../../..
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
		dotest 43.5 "${testcvs} -q update first-dir" ''

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
		if [ -d first-dir ] ; then
			echo "FAIL: test 45.5" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 45.5" >>${LOGFILE}
		fi

		# now try some rtags

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
		dotest 56 "${testcvs} import -m first-import second-dir first-immigration immigration1 immigration1_0" \
'N second-dir/file14
N second-dir/file6
N second-dir/file7
'"${PROG}"' [a-z]*: Importing /tmp/cvs-sanity/cvsroot/second-dir/dir1
N second-dir/dir1/file14
N second-dir/dir1/file6
N second-dir/dir1/file7
'"${PROG}"' [a-z]*: Importing /tmp/cvs-sanity/cvsroot/second-dir/dir1/dir2
N second-dir/dir1/dir2/file14
N second-dir/dir1/dir2/file6
N second-dir/dir1/dir2/file7

No conflicts created by this import'

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

		rm -rf second-dir
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

		dotest 61 "${testcvs} -q diff -u" ''

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

		# Test the cvs history command.

		# The reason that there are two patterns rather than using
		# \(/tmp/cvs-sanity\|<remote>\) is that we are trying to
		# make this portable.  Perhaps at some point we should
		# ditch that notion and require GNU expr (or dejagnu or....)
		# since it seems to be so painful.

		# why are there two lines at the end of the local output
		# which don't exist in the remote output?  would seem to be
		# a CVS bug.
		dotest basic2-64 "${testcvs} his -e -a" \
'O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir           =first-dir= /tmp/cvs-sanity/\*
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file6     first-dir           == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file7     first-dir           == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file6     first-dir/dir1      == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file7     first-dir/dir1      == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file6     first-dir/dir1/dir2 == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file7     first-dir/dir1/dir2 == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file14    first-dir           == /tmp/cvs-sanity
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir           == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file14    first-dir/dir1      == /tmp/cvs-sanity
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir/dir1      == /tmp/cvs-sanity
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file14    first-dir/dir1/dir2 == /tmp/cvs-sanity
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir/dir1/dir2 == /tmp/cvs-sanity
F [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]*                     =first-dir= /tmp/cvs-sanity/\*
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir \[rtagged-by-revision:1.1\]
O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* \[1.1\] first-dir           =first-dir= /tmp/cvs-sanity/\*
U [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir           == /tmp/cvs-sanity/first-dir
U [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file7     first-dir           == /tmp/cvs-sanity/first-dir' \
'O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir           =first-dir= <remote>/\*
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file7     first-dir           == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file7     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file6     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file7     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file14    first-dir           == <remote>
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file14    first-dir/dir1      == <remote>
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.1 file14    first-dir/dir1/dir2 == <remote>
M [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* 1.2 file6     first-dir/dir1/dir2 == <remote>
F [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]*                     =first-dir= <remote>/\*
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* first-dir \[rtagged-by-revision:1.1\]
O [0-9/]* [0-9:]* '"${PLUS}"'0000 [a-z@][a-z@]* \[1.1\] first-dir           =first-dir= <remote>/\*'

		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf ${CVSROOT_DIRNAME}/second-dir
		;;

	death) # next dive.  test death support.
		mkdir  ${CVSROOT_DIRNAME}/first-dir
		if ${CVS} co first-dir  ; then
			echo "PASS: test 65" >>${LOGFILE}
		else
			echo "FAIL: test 65" | tee -a ${LOGFILE} ; exit 1
		fi

		cd first-dir

		# Create a directory with only dead files, to make sure CVS
		# doesn't get confused by it.
		mkdir subdir
		dotest 65a0 "${testcvs} add subdir" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/subdir added to the repository'
		cd subdir
		echo file in subdir >sfile
		dotest 65a1 "${testcvs} add sfile" \
"${PROG}"' [a-z]*: scheduling file `sfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
		dotest 65a2 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/subdir/sfile,v
done
Checking in sfile;
/tmp/cvs-sanity/cvsroot/first-dir/subdir/sfile,v  <--  sfile
initial revision: 1.1
done'
		rm sfile
		dotest 65a3 "${testcvs} rm sfile" \
"${PROG}"' [a-z]*: scheduling `sfile'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG}"' commit'\'' to remove this file permanently'
		dotest 65a4 "${testcvs} -q ci -m remove-it" \
'Removing sfile;
/tmp/cvs-sanity/cvsroot/first-dir/subdir/sfile,v  <--  sfile
new revision: delete; previous revision: 1.1
done'
		cd ..
		dotest 65a5 "${testcvs} -q update -P" ''
		dotest_fail 65a6 "test -d subdir" ''

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

		dotest_fail 69a0 "test -f file1" ''
		# get the old contents of file1 back
		if ${testcvs} update -p -r 1.1 file1 >file1 2>>${LOGFILE}; then
		  pass 69a1
		else
		  fail 69a1
		fi
		dotest 69a2 "cat file1" ''

		# create second file
		touch file2
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

		# file4 will be dead at the time of branching and stay dead.
		echo file4 > file4
		dotest death-file4-add "${testcvs} add file4" \
"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
		dotest death-file4-ciadd "${testcvs} -q ci -m add file4" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1.1
done'
		rm file4
		dotest death-file4-rm "${testcvs} remove file4" \
"${PROG}"' [a-z]*: scheduling `file4'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG}"' commit'\'' to remove this file permanently'
		dotest death-file4-cirm "${testcvs} -q ci -m remove file4" \
'Removing file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1.1
done'

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

		dotest_fail death-file4-3 "test -f file4" ''

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

		dotest_fail death-file4-4 "test -f file4" ''

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

		dotest_fail death-file4-5 "test -f file4" ''

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

		# typo; try to get to the branch and fail
		dotest_fail 92.1a "${testcvs} update -r brnach1" \
		  "${PROG}"' \[[a-z]* aborted\]: no such tag brnach1'
		# Make sure we are still on the trunk
		if test -f file1 ; then
			echo "FAIL: 92.1b" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: 92.1b" >>${LOGFILE}
		fi
		if test -f file2 ; then
			echo "PASS: 92.1c" >>${LOGFILE}
		else
			echo "FAIL: 92.1c" | tee -a ${LOGFILE} ; exit 1
		fi

		# back to branch1
		if ${CVS} update -r branch1  2>> ${LOGFILE}; then
			echo "PASS: test 93" >>${LOGFILE}
		else
			echo "FAIL: test 93" | tee -a ${LOGFILE} ; exit 1
		fi

		dotest_fail death-file4-6 "test -f file4" ''

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

		dotest_fail death-file4-7 "test -f file4" ''

		cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		;;
	branches)
	  # More branch tests, including branches off of branches
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest branches-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 1:ancest >file1
	  echo 2:ancest >file2
	  echo 3:ancest >file3
	  echo 4:trunk-1 >file4
	  dotest branches-2 "${testcvs} add file1 file2 file3 file4" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest branches-3 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file2,v
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file3,v
done
Checking in file3;
/tmp/cvs-sanity/cvsroot/first-dir/file3,v  <--  file3
initial revision: 1.1
done
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1.1
done'
	  echo 4:trunk-2 >file4
	  dotest branches-3.2 "${testcvs} -q ci -m trunk-before-branch" \
'Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1.2; previous revision: 1.1
done'
	  dotest branches-4 "${testcvs} tag -b br1" "${PROG}"' [a-z]*: Tagging \.
T file1
T file2
T file3
T file4'
	  dotest branches-5 "${testcvs} update -r br1" \
"${PROG}"' [a-z]*: Updating \.'
	  echo 1:br1 >file1
	  echo 2:br1 >file2
	  echo 4:br1 >file4
	  dotest branches-6 "${testcvs} -q ci -m modify" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1.1.2.1; previous revision: 1.1
done
Checking in file2;
/tmp/cvs-sanity/cvsroot/first-dir/file2,v  <--  file2
new revision: 1.1.2.1; previous revision: 1.1
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1.2.2.1; previous revision: 1.2
done'
	  dotest branches-7 "${testcvs} -q tag -b brbr" 'T file1
T file2
T file3
T file4'
	  dotest branches-8 "${testcvs} -q update -r brbr" ''
	  echo 1:brbr >file1
	  echo 4:brbr >file4
	  dotest branches-9 "${testcvs} -q ci -m modify" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1.1.2.1.2.1; previous revision: 1.1.2.1
done
Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1.2.2.1.2.1; previous revision: 1.2.2.1
done'
	  dotest branches-10 "cat file1 file2 file3 file4" '1:brbr
2:br1
3:ancest
4:brbr'
	  dotest branches-11 "${testcvs} -q update -r br1" \
'[UP] file1
[UP] file4'
	  dotest branches-12 "cat file1 file2 file3 file4" '1:br1
2:br1
3:ancest
4:br1'
	  echo 4:br1-2 >file4
	  dotest branches-12.2 "${testcvs} -q ci -m change-on-br1" \
'Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1.2.2.2; previous revision: 1.2.2.1
done'
	  dotest branches-13 "${testcvs} -q update -A" '[UP] file1
[UP] file2
[UP] file4'
	  dotest branches-14 "cat file1 file2 file3 file4" '1:ancest
2:ancest
3:ancest
4:trunk-2'
	  echo 4:trunk-3 >file4
	  dotest branches-14.2 \
	    "${testcvs} -q ci -m trunk-change-after-branch" \
'Checking in file4;
/tmp/cvs-sanity/cvsroot/first-dir/file4,v  <--  file4
new revision: 1.3; previous revision: 1.2
done'
	  dotest branches-14.3 "${testcvs} log file4" \
'
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
Working file: file4
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
	brbr: 1\.2\.2\.1\.0\.2
	br1: 1\.2\.0\.2
keyword substitution: kv
total revisions: 6;	selected revisions: 6
description:
----------------------------
revision 1\.3
date: [0-9/: ]*;  author: [a-z@][a-z@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
trunk-change-after-branch
----------------------------
revision 1\.2
date: [0-9/: ]*;  author: [a-z@][a-z@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
branches:  1\.2\.2;
trunk-before-branch
----------------------------
revision 1\.1
date: [0-9/: ]*;  author: [a-z@][a-z@]*;  state: Exp;
add-it
----------------------------
revision 1\.2\.2\.2
date: [0-9/: ]*;  author: [a-z@][a-z@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
change-on-br1
----------------------------
revision 1\.2\.2\.1
date: [0-9/: ]*;  author: [a-z@][a-z@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
branches:  1\.2\.2\.1\.2;
modify
----------------------------
revision 1\.2\.2\.1\.2\.1
date: [0-9/: ]*;  author: [a-z@][a-z@]*;  state: Exp;  lines: '"${PLUS}"'1 -1
modify
============================================================================='
	  dotest_status branches-14.4 1 \
	    "${testcvs} diff -c -r 1.1 -r 1.3 file4" \
'Index: file4
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.3
diff -c -r1\.1 -r1\.3
\*\*\* file4	[0-9/]* [0-9:]*	1\.1
--- file4	[0-9/]* [0-9:]*	1\.3
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:trunk-3'
	  dotest_status branches-14.5 1 \
	    "${testcvs} diff -c -r 1.1 -r 1.2.2.1 file4" \
'Index: file4
===================================================================
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.2\.2\.1
diff -c -r1\.1 -r1\.2\.2\.1
\*\*\* file4	[0-9/]* [0-9:]*	1\.1
--- file4	[0-9/]* [0-9:]*	1\.2\.2\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:br1'
	  dotest branches-15 \
	    "${testcvs} update -j 1.1.2.1 -j 1.1.2.1.2.1 file1" \
	    'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
retrieving revision 1.1.2.1
retrieving revision 1.1.2.1.2.1
Merging differences between 1.1.2.1 and 1.1.2.1.2.1 into file1
rcsmerge: warning: conflicts during merge'
	  dotest branches-16 "cat file1" '<<<<<<< file1
1:ancest
=======
1:brbr
>>>>>>> 1.1.2.1.2.1'
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping /tmp/cvs-sanity and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	import) # test death after import
		# import
		mkdir import-dir ; cd import-dir

		for i in 1 2 3 4 ; do
			echo imported file"$i" > imported-file"$i"
		done

		# This directory should be on the default ignore list,
		# so it shouldn't get imported.
		mkdir RCS
		echo ignore.me >RCS/ignore.me

		echo 'import should not expand $''Id$' >>imported-file2
		cp imported-file2 ../imported-file2-orig.tmp

		if ${CVS} import -m first-import first-dir vendor-branch junk-1_0  ; then
			echo "PASS: test 96" >>${LOGFILE}
		else
			echo "FAIL: test 96" | tee -a ${LOGFILE} ; exit 1
		fi

		if cmp ../imported-file2-orig.tmp imported-file2; then
		  pass 96.5
		else
		  fail 96.5
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
		if test -d RCS; then
		  echo "FAIL: test 98.5" | tee -a ${LOGFILE} ; exit 1
		else
		  echo "PASS: test 98.5" >>${LOGFILE}
		fi

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
		cp imported-file2 ../imported-file2-orig.tmp

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0  ; then
			echo "PASS: test 106" >>${LOGFILE}
		else
			echo "FAIL: test 106" | tee -a ${LOGFILE} ; exit 1
		fi
		if cmp ../imported-file2-orig.tmp imported-file2; then
		  pass 106.5
		else
		  fail 106.5
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
		rm -rf import-dir
		;;

	new) # look for stray "no longer pertinent" messages.
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
		mkdir dir1
		dotest conflicts-127b "${testcvs} add dir1" \
'Directory /tmp/cvs-sanity/cvsroot/first-dir/dir1 added to the repository'
		dotest conflicts-128 "${testcvs} -q ci -m changed" \
'Checking in a;
/tmp/cvs-sanity/cvsroot/first-dir/a,v  <--  a
new revision: 1.2; previous revision: 1.1
done'
		cd ../../2/first-dir
		echo add a conflicting line >>a
		dotest_fail conflicts-129 "${testcvs} -q ci -m changed" \
"${PROG}"' [a-z]*: Up-to-date check failed for `a'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
		mkdir dir1
		mkdir sdir
		dotest conflicts-130 "${testcvs} -q update" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
retrieving revision 1.1
retrieving revision 1.2
Merging differences between 1.1 and 1.2 into a
rcsmerge: warning: conflicts during merge
'"${PROG}"' [a-z]*: conflicts found in a
C a
'"${QUESTION}"' dir1
'"${QUESTION}"' sdir' \
''"${QUESTION}"' dir1
'"${QUESTION}"' sdir
RCS file: /tmp/cvs-sanity/cvsroot/first-dir/a,v
retrieving revision 1.1
retrieving revision 1.2
Merging differences between 1.1 and 1.2 into a
rcsmerge: warning: conflicts during merge
'"${PROG}"' [a-z]*: conflicts found in a
C a'

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
		dotest conflicts-136 "${testcvs} -q update" \
'[UP] first-dir/abc
'"${QUESTION}"' first-dir/dir1
'"${QUESTION}"' first-dir/sdir' \
''"${QUESTION}"' first-dir/dir1
'"${QUESTION}"' first-dir/sdir
[UP] first-dir/abc'
		dotest conflicts-137 'test -f first-dir/abc' ''
		rmdir first-dir/dir1 first-dir/sdir

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

	  mkdir ssdir
	  ${testcvs} add ssdir >>${LOGFILE}

	  touch a b

	  if ${testcvs} add a b 2>>${LOGFILE} ; then
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
	  echo dirmodule first-dir/subdir >>CVSROOT/modules
	  echo namedmodule -d nameddir first-dir/subdir >>CVSROOT/modules
	  echo aliasmodule -a first-dir/subdir/a >>CVSROOT/modules
	  echo aliasnested -a first-dir/subdir/ssdir >>CVSROOT/modules

	  # Options must come before arguments.  It is possible this should
	  # be relaxed at some point (though the result would be bizarre for
	  # -a); for now test the current behavior.
	  echo bogusalias first-dir/subdir/a -a >>CVSROOT/modules
	  if ${testcvs} ci -m 'add modules' CVSROOT/modules \
	      >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 148' >>${LOGFILE}
	  else
	    echo 'FAIL: test 148' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cd ..
	  dotest 148a0 "${testcvs} co -c" 'CVSROOT      CVSROOT
aliasmodule  -a first-dir/subdir/a
aliasnested  -a first-dir/subdir/ssdir
bogusalias   first-dir/subdir/a -a
dirmodule    first-dir/subdir
namedmodule  -d nameddir first-dir/subdir
realmodule   first-dir/subdir a'
	  # I don't know why aliasmodule isn't printed (I would have thought
	  # that it gets printed without the -a; although I'm not sure that
	  # printing expansions without options is useful).
	  dotest 148a1 "${testcvs} co -s" 'CVSROOT      NONE        CVSROOT
bogusalias   NONE        first-dir/subdir/a -a
dirmodule    NONE        first-dir/subdir
namedmodule  NONE        first-dir/subdir
realmodule   NONE        first-dir/subdir a'

	  # Test that real modules check out to realmodule/a, not subdir/a.
	  if ${testcvs} co realmodule >>${LOGFILE}; then
	    echo 'PASS: test 149a1' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a1' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d realmodule && test -f realmodule/a; then
	    echo 'PASS: test 149a2' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a2' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -f realmodule/b; then
	    echo 'FAIL: test 149a3' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 149a3' >>${LOGFILE}
	  fi
	  if ${testcvs} -q co realmodule; then
	    echo 'PASS: test 149a4' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a4' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if echo "yes" | ${testcvs} release -d realmodule >>${LOGFILE} ; then
	    echo 'PASS: test 149a5' >>${LOGFILE}
	  else
	    echo 'FAIL: test 149a5' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  # Now test the ability to check out a single file from a directory
	  if ${testcvs} co dirmodule/a >>${LOGFILE}; then
	    echo 'PASS: test 150c' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150c' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -d dirmodule && test -f dirmodule/a; then
	    echo 'PASS: test 150d' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150d' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if test -f dirmodule/b; then
	    echo 'FAIL: test 150e' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 150e' >>${LOGFILE}
	  fi
	  if echo "yes" | ${testcvs} release -d dirmodule >>${LOGFILE} ; then
	    echo 'PASS: test 150f' >>${LOGFILE}
	  else
	    echo 'FAIL: test 150f' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  # Now test the ability to correctly reject a non-existent filename.
	  # For maximum studliness we would check that an error message is
	  # being output.
	  if ${testcvs} co dirmodule/nonexist >>${LOGFILE} 2>&1; then
	    # We accept a zero exit status because it is what CVS does
	    # (Dec 95).  Probably the exit status should be nonzero,
	    # however.
	    echo 'PASS: test 150g1' >>${LOGFILE}
	  else
	    echo 'PASS: test 150g1' >>${LOGFILE}
	  fi
	  # We tolerate the creation of the dirmodule directory, since that
	  # is what CVS does, not because we view that as preferable to not
	  # creating it.
	  if test -f dirmodule/a || test -f dirmodule/b; then
	    echo 'FAIL: test 150g2' | tee -a ${LOGFILE}
	    exit 1
	  else
	    echo 'PASS: test 150g2' >>${LOGFILE}
	  fi
	  rm -rf dirmodule

	  # Now test that a module using -d checks out to the specified
	  # directory.
	  dotest 150h1 "${testcvs} -q co namedmodule" 'U nameddir/a
U nameddir/b'
	  if test -f nameddir/a && test -f nameddir/b; then
	    pass 150h2
	  else
	    fail 150h2
	  fi
	  echo add line >>nameddir/a
	  dotest 150h3 "${testcvs} -q co namedmodule" 'M nameddir/a'
	  rm nameddir/a
	  dotest 150h4 "${testcvs} -q co namedmodule" 'U nameddir/a'
	  if echo "yes" | ${testcvs} release -d nameddir >>${LOGFILE} ; then
	    pass 150h99
	  else
	    fail 150h99
	  fi

	  # Now test that alias modules check out to subdir/a, not
	  # aliasmodule/a.
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

	  cd ..
	  rm -rf 1

	  mkdir 2
	  cd 2
	  dotest modules-155a0 "${testcvs} co aliasnested" \
"${PROG} [a-z]*: Updating first-dir/subdir/ssdir"
	  dotest modules-155a1 "test -d first-dir" ''
	  dotest modules-155a2 "test -d first-dir/subdir" ''
	  dotest modules-155a3 "test -d first-dir/subdir/ssdir" ''
	  # Test that nothing extraneous got created.
	  dotest modules-155a4 "ls -1" "first-dir"
	  cd ..
	  rm -rf 2

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
	  # FIXME: should be using dotest and PROG.
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

	devcom)
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co first-dir >>${LOGFILE} ; then
	    echo 'PASS: test 169' >>${LOGFILE}
	  else
	    echo 'FAIL: test 169' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  cd first-dir
	  echo abb >abb
	  if ${testcvs} add abb 2>>${LOGFILE}; then
	    echo 'PASS: test 170' >>${LOGFILE}
	  else
	    echo 'FAIL: test 170' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 171' >>${LOGFILE}
	  else
	    echo 'FAIL: test 171' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  dotest_fail 171a0 "${testcvs} watch" "Usage${DOTSTAR}"
	  if ${testcvs} watch on; then
	    echo 'PASS: test 172' >>${LOGFILE}
	  else
	    echo 'FAIL: test 172' | tee -a ${LOGFILE}
	  fi
	  echo abc >abc
	  if ${testcvs} add abc 2>>${LOGFILE}; then
	    echo 'PASS: test 173' >>${LOGFILE}
	  else
	    echo 'FAIL: test 173' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 174' >>${LOGFILE}
	  else
	    echo 'FAIL: test 174' | tee -a ${LOGFILE}
	  fi

	  cd ../..
	  mkdir 2
	  cd 2

	  if ${testcvs} -q co first-dir >>${LOGFILE}; then
	    echo 'PASS: test 175' >>${LOGFILE}
	  else
	    echo 'FAIL: test 175' | tee -a ${LOGFILE}
	  fi
	  cd first-dir
	  if test -w abb; then
	    echo 'FAIL: test 176' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 176' >>${LOGFILE}
	  fi
	  if test -w abc; then
	    echo 'FAIL: test 177' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 177' >>${LOGFILE}
	  fi

	  if ${testcvs} editors >../ans178.tmp; then
	    echo 'PASS: test 178' >>${LOGFILE}
	  else
	    echo 'FAIL: test 178' | tee -a ${LOGFILE}
	  fi
	  cat ../ans178.tmp >>${LOGFILE}
	  if test -s ../ans178.tmp; then
	    echo 'FAIL: test 178a' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 178a' >>${LOGFILE}
	  fi

	  if ${testcvs} edit abb; then
	    echo 'PASS: test 179' >>${LOGFILE}
	  else
	    echo 'FAIL: test 179' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  if ${testcvs} editors >../ans180.tmp; then
	    echo 'PASS: test 180' >>${LOGFILE}
	  else
	    echo 'FAIL: test 180' | tee -a ${LOGFILE}
	    exit 1
	  fi
	  cat ../ans180.tmp >>${LOGFILE}
	  if test -s ../ans180.tmp; then
	    echo 'PASS: test 181' >>${LOGFILE}
	  else
	    echo 'FAIL: test 181' | tee -a ${LOGFILE}
	  fi

	  echo aaaa >>abb
	  if ${testcvs} ci -m modify abb >>${LOGFILE} 2>&1; then
	    echo 'PASS: test 182' >>${LOGFILE}
	  else
	    echo 'FAIL: test 182' | tee -a ${LOGFILE}
	  fi
	  # Unedit of a file not being edited should be a noop.
	  dotest 182.5 "${testcvs} unedit abb" ''

	  if ${testcvs} editors >../ans183.tmp; then
	    echo 'PASS: test 183' >>${LOGFILE}
	  else
	    echo 'FAIL: test 183' | tee -a ${LOGFILE}
	  fi
	  cat ../ans183.tmp >>${LOGFILE}
	  if test -s ../ans183.tmp; then
	    echo 'FAIL: test 184' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 184' >>${LOGFILE}
	  fi

	  if test -w abb; then
	    echo 'FAIL: test 185' | tee -a ${LOGFILE}
	  else
	    echo 'PASS: test 185' >>${LOGFILE}
	  fi

	  if ${testcvs} edit abc; then
	    echo 'PASS: test 186a1' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a1' | tee -a ${LOGFILE}
	  fi
	  # Unedit of an unmodified file.
	  if ${testcvs} unedit abc; then
	    echo 'PASS: test 186a2' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a2' | tee -a ${LOGFILE}
	  fi
	  if ${testcvs} edit abc; then
	    echo 'PASS: test 186a3' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a3' | tee -a ${LOGFILE}
	  fi
	  echo changedabc >abc
	  # Try to unedit a modified file; cvs should ask for confirmation
	  if (echo no | ${testcvs} unedit abc) >>${LOGFILE}; then
	    echo 'PASS: test 186a4' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a4' | tee -a ${LOGFILE}
	  fi
	  if echo changedabc | cmp - abc; then
	    echo 'PASS: test 186a5' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a5' | tee -a ${LOGFILE}
	  fi
	  # OK, now confirm the unedit
	  if (echo yes | ${testcvs} unedit abc) >>${LOGFILE}; then
	    echo 'PASS: test 186a6' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a6' | tee -a ${LOGFILE}
	  fi
	  if echo abc | cmp - abc; then
	    echo 'PASS: test 186a7' >>${LOGFILE}
	  else
	    echo 'FAIL: test 186a7' | tee -a ${LOGFILE}
	  fi

	  dotest devcom-a0 "${testcvs} watchers" ''
	  dotest devcom-a1 "${testcvs} watch add" ''
	  dotest devcom-a2 "${testcvs} watchers" \
'abb	[a-z0-9]*	edit	unedit	commit
abc	[a-z0-9]*	edit	unedit	commit'
	  dotest devcom-a3 "${testcvs} watch remove -a unedit abb" ''
	  dotest devcom-a4 "${testcvs} watchers abb" \
'abb	[a-z0-9]*	edit	commit'

	  cd ../..
	  rm -rf 1 2 ${CVSROOT_DIRNAME}/first-dir
	  ;;

	ignore)
	  dotest 187a1 "${testcvs} -q co CVSROOT" 'U CVSROOT/modules'
	  cd CVSROOT
	  echo rootig.c >cvsignore
	  dotest 187a2 "${testcvs} add cvsignore" "${PROG}"' [a-z]*: scheduling file `cvsignore'"'"' for addition
'"${PROG}"' [a-z]*: use '"'"'cvs commit'"'"' to add this file permanently'

	  # As of Jan 96, local CVS prints "Examining ." and remote doesn't.
	  # Accept either.
	  dotest 187a3 " ${testcvs} ci -m added" \
"${DOTSTAR}"'CS file: /tmp/cvs-sanity/cvsroot/CVSROOT/cvsignore,v
done
Checking in cvsignore;
/tmp/cvs-sanity/cvsroot/CVSROOT/cvsignore,v  <--  cvsignore
initial revision: 1.1
done
'"${PROG}"' [a-z]*: Rebuilding administrative file database'

	  cd ..
	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	    echo 'PASS: test 187a4' >>${LOGFILE}
	  else
	    echo 'FAIL: test 187a4' | tee -a ${LOGFILE}
	    exit 1
	  fi

	  # CVS looks at the home dir from getpwuid, not HOME (is that correct
	  # behavior?), so this is hard to test and we won't try.
	  # echo foobar.c >${HOME}/.cvsignore
	  CVSIGNORE=envig.c; export CVSIGNORE
	  mkdir dir-to-import
	  cd dir-to-import
	  touch foobar.c bar.c rootig.c defig.o envig.c optig.c
	  # We really should allow the files to be listed in any order.
	  # But we (kludgily) just list the orders which have been observed.
	  dotest 188a "${testcvs} import -m m -I optig.c first-dir tag1 tag2" \
	    'N first-dir/foobar.c
N first-dir/bar.c
I first-dir/rootig.c
I first-dir/defig.o
I first-dir/envig.c
I first-dir/optig.c

No conflicts created by this import' 'I first-dir/defig.o
I first-dir/envig.c
I first-dir/optig.c
N first-dir/foobar.c
N first-dir/bar.c
I first-dir/rootig.c

No conflicts created by this import'
	  dotest 188b "${testcvs} import -m m -I ! second-dir tag3 tag4" \
	    'N second-dir/foobar.c
N second-dir/bar.c
N second-dir/rootig.c
N second-dir/defig.o
N second-dir/envig.c
N second-dir/optig.c

No conflicts created by this import'
	  cd ..
	  rm -rf dir-to-import

	  dotest 189a "${testcvs} -q co second-dir" \
'U second-dir/bar.c
U second-dir/defig.o
U second-dir/envig.c
U second-dir/foobar.c
U second-dir/optig.c
U second-dir/rootig.c'
	  rm -rf second-dir
	  dotest 189b "${testcvs} -q co first-dir" 'U first-dir/bar.c
U first-dir/foobar.c'
	  cd first-dir
	  touch rootig.c defig.o envig.c optig.c notig.c
	  dotest 189c "${testcvs} -q update -I optig.c" "${QUESTION} notig.c"
	  # The fact that CVS requires us to specify -I CVS here strikes me
	  # as a bug.
	  dotest 189d "${testcvs} -q update -I ! -I CVS" "${QUESTION} rootig.c
${QUESTION} defig.o
${QUESTION} envig.c
${QUESTION} optig.c
${QUESTION} notig.c"
	  cd ..
	  rm -rf first-dir

	  rm -rf ${CVSROOT_DIRNAME}/first-dir ${CVSROOT_DIRNAME}/second-dir
	  ;;

	binfiles)
	  # Test cvs's ability to handle binary files.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest binfiles-1 "${testcvs} -q co first-dir" ''
	  awk 'BEGIN { printf "%c%c%c%c%c%c", 2, 10, 137, 0, 13, 10 }' \
	    </dev/null >binfile.dat
	  cat binfile.dat binfile.dat >binfile2.dat
	  cd first-dir
	  cp ../binfile.dat binfile
	  dotest binfiles-2 "${testcvs} add -kb binfile" \
"${PROG}"' [a-z]*: scheduling file `binfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest binfiles-3 "${testcvs} -q ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/binfile,v
done
Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
initial revision: 1.1
done'
	  cd ../..
	  mkdir 2; cd 2
	  dotest binfiles-4 "${testcvs} -q co first-dir" 'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-5 "cmp ../../1/binfile.dat binfile" ''
	  # Testing that sticky options is -kb is the closest thing we have
	  # to testing that binary files work right on non-unix machines
	  # (until there is automated testing for such machines, of course).
	  dotest binfiles-5.5 "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-6 "${testcvs} -q ci -m modify-it" \
'Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1.2; previous revision: 1.1
done'
	  cd ../../1/first-dir
	  dotest binfiles-7 "${testcvs} -q update" '[UP] binfile'
	  dotest binfiles-8 "cmp ../binfile2.dat binfile" ''

	  # The bugs which these test for are apparently not fixed for remote.
	  if test "$remote" = no; then
	    dotest binfiles-9 "${testcvs} -q update -A" ''
	    dotest binfiles-10 "${testcvs} -q update -kk" '[UP] binfile'
	    dotest binfiles-11 "${testcvs} -q update" ''
	    dotest binfiles-12 "${testcvs} -q update -A" '[UP] binfile'
	    dotest binfiles-13 "${testcvs} -q update -A" ''
	  fi

	  cd ../../2/first-dir
	  echo 'this file is $''RCSfile$' >binfile
	  dotest binfiles-14a "${testcvs} -q ci -m modify-it" \
'Checking in binfile;
/tmp/cvs-sanity/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1.3; previous revision: 1.2
done'
	  dotest binfiles-14b "cat binfile" 'this file is $''RCSfile$'
	  # See binfiles-5.5 for discussion of -kb.
	  dotest binfiles-14c "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.3.*
   Repository revision:	1\.3	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  dotest binfiles-14d "${testcvs} admin -kv binfile" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/binfile,v
done'
	  # cvs admin doesn't change the checked-out file or its sticky
	  # kopts.  There probably should be a way which does (but
	  # what if the file is modified?  And do we try to version
	  # control the kopt setting?)
	  dotest binfiles-14e "cat binfile" 'this file is $''RCSfile$'
	  dotest binfiles-14f "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.3.*
   Repository revision:	1\.3	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb'
	  dotest binfiles-14g "${testcvs} -q update -A" '[UP] binfile'
	  dotest binfiles-14h "cat binfile" 'this file is binfile,v'
	  dotest binfiles-14i "${testcvs} status binfile" \
'===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.3.*
   Repository revision:	1\.3	/tmp/cvs-sanity/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kv'

	  cd ../..
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 1 2
	  ;;
	info)
	  # Test CVS's ability to handle *info files.
	  dotest info-1 "${testcvs} -q co CVSROOT" "[UP] CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  echo "ALL sh -c \"echo x\${=MYENV}\${=OTHER}y\${=ZEE}=\$USER=\$CVSROOT= >>$TESTDIR/testlog; cat >/dev/null\"" > loginfo
	  dotest info-2 "${testcvs} add loginfo" \
"${PROG}"' [a-z]*: scheduling file `loginfo'"'"' for addition
'"${PROG}"' [a-z]*: use '"'"'cvs commit'"'"' to add this file permanently'
	  dotest info-3 "${testcvs} -q ci -m new-loginfo" \
'RCS file: /tmp/cvs-sanity/cvsroot/CVSROOT/loginfo,v
done
Checking in loginfo;
/tmp/cvs-sanity/cvsroot/CVSROOT/loginfo,v  <--  loginfo
initial revision: 1.1
done
'"${PROG}"' [a-z]*: Rebuilding administrative file database'
	  cd ..
	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	    pass info-4
	  else
	    fail info-4
	  fi

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest info-5 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  touch file1
	  dotest info-6 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  echo "cvs -s OTHER=not-this -s MYENV=env-" >>$HOME/.cvsrc
	  dotest info-6a "${testcvs} -q -s OTHER=value ci -m add-it" \
'RCS file: /tmp/cvs-sanity/cvsroot/first-dir/file1,v
done
Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1.1
done
'"${PROG}"' [a-z]*: loginfo:1: no such user variable ${=ZEE}'
	  echo line1 >>file1
	  dotest info-7 "${testcvs} -q -s OTHER=value -s ZEE=z ci -m mod-it" \
'Checking in file1;
/tmp/cvs-sanity/cvsroot/first-dir/file1,v  <--  file1
new revision: 1.2; previous revision: 1.1
done'
	  cd ..
	  if echo "yes" | ${testcvs} release -d first-dir >>${LOGFILE} ; then
	    pass info-8
	  else
	    fail info-8
	  fi
	  dotest info-9 "cat $TESTDIR/testlog" 'xenv-valueyz=[a-z@][a-z@]*=/tmp/cvs-sanity/cvsroot='

	  # I think this might be doable with cvs remove, or at least
	  # checking in a version with only comments, but I'm too lazy
	  # at the moment.  Blow it away.
	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/loginfo*

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
# * Test ability to send notifications in response to watches.  (currently
#   hard to test because CVS doesn't send notifications if username is the
#   same).
# * Test that remote edit and/or unedit works when disconnected from
#   server (e.g. set CVS_SERVER to "foobar").
# End of TODO list.

# Remove the test directory, but first change out of it.
cd /tmp
rm -rf ${TESTDIR}

# end of sanity.sh
