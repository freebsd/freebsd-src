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

# You can't run CVS as root; print a nice error message here instead
# of somewhere later, after making a mess.
case "`whoami`" in
  "root" )
    echo "sanity.sh: test suite does not work correctly when run as root" >&2
    exit 1
  ;;
esac

# required to make this script work properly.
unset CVSREAD

TESTDIR=${TESTDIR:-/tmp/cvs-sanity}

# "debugger"
#set -x

echo 'This test should produce no other output than this line, and a final "OK".'

if test x"$1" = x"-r"; then
	shift
	remote=yes
	# If we're going to do remote testing, make sure 'rsh' works first.
        host="`hostname`"
	if test "x`${CVS_RSH-rsh} $host -n 'echo hi'`" != "xhi"; then
	    echo "ERROR: cannot test remote CVS, because \`rsh $host' fails." >&2
	    exit 1
	fi
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

# Regexp to match an author name.  I'm not really sure what characters
# should be here.  a-zA-Z obviously.  People complained when 0-9 were
# not allowed in usernames.  Other than that I'm not sure.
username="[a-zA-Z0-9][a-zA-Z0-9]*"

# Regexp to match the name of a temporary file (from cvs_temp_name).
# This appears in certain diff output.
tempname="[-a-zA-Z0-9/.%_]*"

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

GEXPRLOCS="`echo $PATH | sed 's/:/ /g'` /usr/local/bin /usr/contrib/bin /usr/gnu/bin /local/bin /local/gnu/bin /gun/bin"

EXPR=expr

# Cause NextStep 3.3 users to lose in a more graceful fashion.
if $EXPR 'abc
def' : 'abc
def' >/dev/null; then
  : good, it works
else
  for path in $GEXPRLOCS ; do
    if test -x $path/gexpr ; then
      if test "X`$path/gexpr --version`" != "X--version" ; then
        EXPR=$path/gexpr
        break
      fi
    fi
    if test -x $path/expr ; then
      if test "X`$path/expr --version`" != "X--version" ; then
        EXPR=$path/expr
        break
      fi
    fi
  done
  if test -z "$EXPR" ; then
    echo 'Running these tests requires an "expr" program that can handle'
    echo 'multi-line patterns.  Make sure that such an expr (GNU, or many but'
    echo 'not all vendor-supplied versions) is in your path.'
    exit 1
  fi
fi

# Warn SunOS, SysVr3.2, etc., users that they may be partially losing
# if we can't find a GNU expr to ease their troubles...
if $EXPR 'a
b' : 'a
c' >/dev/null; then
  for path in $GEXPRLOCS ; do
    if test -x $path/gexpr ; then
      if test "X`$path/gexpr --version`" != "X--version" ; then
        EXPR=$path/gexpr
        break
      fi
    fi
    if test -x $path/expr ; then
      if test "X`$path/expr --version`" != "X--version" ; then
        EXPR=$path/expr
        break
      fi
    fi
  done
  if test -z "$EXPR" ; then
    echo 'Warning: you are using a version of expr which does not correctly'
    echo 'match multi-line patterns.  Some tests may spuriously pass.'
    echo 'You may wish to make sure GNU expr is in your path.'
    EXPR=expr
  fi
else
  : good, it works
fi

# That we should have to do this is total bogosity, but GNU expr
# version 1.9.4-1.12 uses the emacs definition of "$" instead of the unix
# (e.g. SunOS 4.1.3 expr) one.  Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
ENDANCHOR="$"
if $EXPR 'abc
def' : 'abc$' >/dev/null; then
  ENDANCHOR='\'\'
fi

# Work around another GNU expr (version 1.10-1.12) bug/incompatibility.
# "." doesn't appear to match a newline (it does with SunOS 4.1.3 expr).
# Note that the workaround is not a complete equivalent of .* because
# the first parenthesized expression in the regexp must match something
# in order for expr to return a successful exit status.
# Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
DOTSTAR='.*'
if $EXPR 'abc
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
if $EXPR 'a +b' : "a ${PLUS}b" >/dev/null; then
  : good, it works
else
  PLUS='\+'
fi

# Likewise, for ?
QUESTION='?'
if $EXPR 'a?b' : "a${QUESTION}b" >/dev/null; then
  : good, it works
else
  QUESTION='\?'
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
      echo "$3" > ${TESTDIR}/dotest.exp
      rm -f ${TESTDIR}/dotest.ex2
      echo "** got: " >>${LOGFILE}
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      fail "$1"
    else
      pass "$1"
    fi
  else
    if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : \
	"$3"${ENDANCHOR} >/dev/null; then
      # See below about writing this to the logfile.
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      pass "$1"
    else
      if test x"$4" != x; then
	if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : \
	    "$4"${ENDANCHOR} >/dev/null; then
	  # Why, I hear you ask, do we write this to the logfile
	  # even when the test passes?  The reason is that the test
	  # may give us the regexp which we were supposed to match,
	  # but sometimes it may be useful to look at the exact
	  # text which was output.  For example, suppose one wants
	  # to grep for a particular warning, and make _sure_ that
	  # CVS never hits it (even in cases where the tests might
	  # match it with .*).  Or suppose one wants to see the exact
	  # date format output in a certain case (where the test will
	  # surely use a somewhat non-specific pattern).
	  cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	  pass "$1"
	else
	  echo "** expected: " >>${LOGFILE}
	  echo "$3" >>${LOGFILE}
	  echo "$3" > ${TESTDIR}/dotest.ex1
	  echo "** or: " >>${LOGFILE}
	  echo "$4" >>${LOGFILE}
	  echo "$4" > ${TESTDIR}/dotest.ex2
	  echo "** got: " >>${LOGFILE}
	  cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	  fail "$1"
	fi
      else
	echo "** expected: " >>${LOGFILE}
	echo "$3" >>${LOGFILE}
	echo "$3" > ${TESTDIR}/dotest.exp
	echo "** got: " >>${LOGFILE}
	cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
	fail "$1"
      fi
    fi
  fi
}

dotest_all_in_one ()
{
  if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : \
         "`cat ${TESTDIR}/dotest.exp`" >/dev/null; then
    return 0
  fi
  return 1
}

# WARNING: this won't work with REs that match newlines....
#
dotest_line_by_line ()
{
  line=1
  while [ $line -le `wc -l ${TESTDIR}/dotest.tmp` ] ; do
    echo "$line matched \c" >>$LOGFILE
    if $EXPR "`sed -n ${line}p ${TESTDIR}/dotest.tmp`" : \
       "`sed -n ${line}p ${TESTDIR}/dotest.exp`" >/dev/null; then
      :
    else
      echo "**** expected line: " >>${LOGFILE}
      sed -n ${line}p ${TESTDIR}/dotest.exp >>${LOGFILE}
      echo "**** got line: " >>${LOGFILE}
      sed -n ${line}p ${TESTDIR}/dotest.tmp >>${LOGFILE}
      unset line
      return 1
    fi
    line=`expr $line + 1`
  done
  unset line
  return 0
}

# If you are having trouble telling which line of a multi-line
# expression is not being matched, replace calls to dotest_internal()
# with calls to this function:
#
dotest_internal_debug ()
{
  if test -z "$3"; then
    if test -s ${TESTDIR}/dotest.tmp; then
      echo "** expected: " >>${LOGFILE}
      echo "$3" >>${LOGFILE}
      echo "$3" > ${TESTDIR}/dotest.exp
      rm -f ${TESTDIR}/dotest.ex2
      echo "** got: " >>${LOGFILE}
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      fail "$1"
    else
      pass "$1"
    fi
  else
    echo "$3" > ${TESTDIR}/dotest.exp
    if dotest_line_by_line "$1" "$2"; then
      pass "$1"
    else
      if test x"$4" != x; then
	mv ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.ex1
	echo "$4" > ${TESTDIR}/dotest.exp
	if dotest_line_by_line "$1" "$2"; then
	  pass "$1"
	else
	  mv ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.ex2
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
  rm -f ${TESTDIR}/dotest.ex? 2>&1
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

# Like dotest except only 2 args and result must exactly match stdin
dotest_lit ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
  if $2 >${TESTDIR}/dotest.tmp 2>&1; then
    : so far so good
  else
    status=$?
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  cat >${TESTDIR}/dotest.exp
  if cmp ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.tmp >/dev/null 2>&1; then
    pass "$1"
  else
    echo "** expected: " >>${LOGFILE}
    cat ${TESTDIR}/dotest.exp >>${LOGFILE}
    echo "** got: " >>${LOGFILE}
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    fail "$1"
  fi
}

# Like dotest except exitstatus should be nonzero.
dotest_fail ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
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
# This will show up in cvs history output where it prints the working
# directory.  It should *not* appear in any cvs output referring to the
# repository; cvs should use the name of the repository as specified.
TMPPWD=`/bin/pwd`

# Avoid picking up any stray .cvsrc, etc., from the user running the tests
mkdir home
HOME=${TESTDIR}/home; export HOME

# Make sure this variable is not defined to anything that would
# change the format of rcs dates.  Otherwise people using e.g.,
# RCSINIT=-zLT get lots of spurious failures.
RCSINIT=; export RCSINIT

# Remaining arguments are the names of tests to run.
#
# The testsuite is broken up into (hopefully manageably-sized)
# independently runnable tests, so that one can quickly get a result
# from a cvs or testsuite change, and to facilitate understanding the
# tests.

if test x"$*" = x; then
	tests="basica basicb basic1 deep basic2 rdiff death death2 branches"
	tests="${tests} multibranch import join new newb conflicts conflicts2"
	tests="${tests} modules modules2 modules3 mflag errmsg1 devcom devcom2"
	tests="${tests} devcom3 ignore binfiles binfiles2 binwrap mwrap info"
	tests="${tests} serverpatch log log2 crerepos rcs big modes stamps"
	tests="${tests} sticky keyword toplevel"
else
	tests="$*"
fi

# a simple function to compare directory contents
#
# Returns: {nothing}
# Side Effects: ISDIFF := true|false
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
		if test -f $DIR_1/"$a" ; then
			cmp -s $DIR_1/"$a" $DIR_2/"$a"
			if test $? -ne 0 ; then
				ISDIFF=true
			fi
		fi
	done < /tmp/dc$$d1
	rm -f /tmp/dc$$*
}

# Set up CVSROOT (the crerepos tests will test operating without CVSROOT set).
CVSROOT_DIRNAME=${TESTDIR}/cvsroot
CVSROOT=${CVSROOT_DIRNAME} ; export CVSROOT
if test "x$remote" = xyes; then
	# Use rsh so we can test it without having to muck with inetd
	# or anything like that.  Also needed to get CVS_SERVER to
	# work.
	CVSROOT=:ext:`hostname`:${CVSROOT_DIRNAME} ; export CVSROOT
	CVS_SERVER=${testcvs}; export CVS_SERVER
fi

dotest 1 "${testcvs} init" ''

### The big loop
for what in $tests; do
	case $what in
	basica)
	  # Similar in spirit to some of the basic1, and basic2
	  # tests, but hopefully a lot faster.  Also tests operating on
	  # files two directories down *without* operating on the parent dirs.

	  # Tests basica-0a and basica-0b provide the equivalent of the:
	  #    mkdir ${CVSROOT_DIRNAME}/first-dir
	  # used by many of the tests.  It is "more official" in the sense
	  # that is does everything through CVS; the reason most of the
	  # tests don't use it is mostly historical.
	  mkdir 1; cd 1
	  dotest basica-0a "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest basica-0b "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd ..
	  rm -r 1

	  dotest basica-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # Test a few operations, to ensure they gracefully do
	  # nothing in an empty directory.
	  dotest basica-1a0 "${testcvs} -q update" ''
	  dotest basica-1a1 "${testcvs} -q diff -c" ''
	  dotest basica-1a2 "${testcvs} -q status" ''

	  mkdir sdir
	  # Remote CVS gives the "cannot open CVS/Entries" error, which is
	  # clearly a bug, but not a simple one to fix.
	  dotest basica-1a10 "${testcvs} -n add sdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir added to the repository" \
"${PROG} add: cannot open CVS/Entries for reading: No such file or directory
Directory ${TESTDIR}/cvsroot/first-dir/sdir added to the repository"
	  dotest_fail basica-1a11 \
	    "test -d ${CVSROOT_DIRNAME}/first-dir/sdir" ''
	  dotest basica-2 "${testcvs} add sdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir added to the repository"
	  cd sdir
	  mkdir ssdir
	  dotest basica-3 "${testcvs} add ssdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir/ssdir added to the repository"
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
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
done
Checking in sdir/ssdir/ssfile;
${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
initial revision: 1\.1
done"
	  dotest_fail basica-5a \
	    "${testcvs} -q tag BASE sdir/ssdir/ssfile" \
"${PROG} [a-z]*: Attempt to add reserved tag name BASE
${PROG} \[[a-z]* aborted\]: failed to set tag BASE to revision 1\.1 in ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v"
	  dotest basica-5b "${testcvs} -q tag NOT_RESERVED" \
'T sdir/ssdir/ssfile'

	  dotest basica-6 "${testcvs} -q update" ''
	  echo "ssfile line 2" >>sdir/ssdir/ssfile
	  dotest_status basica-6.2 1 "${testcvs} -q diff -c" \
"Index: sdir/ssdir/ssfile
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* ssfile	[0-9/]* [0-9:]*	1\.1
--- ssfile	[0-9/]* [0-9:]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
${PLUS} ssfile line 2"
	  dotest_status basica-6.3 1 "${testcvs} -q diff -c -rBASE" \
"Index: sdir/ssdir/ssfile
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* ssfile	[0-9/]* [0-9:]*	1\.1
--- ssfile	[0-9/]* [0-9:]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
${PLUS} ssfile line 2"
	  dotest basica-7 "${testcvs} -q ci -m modify-it" \
"Checking in sdir/ssdir/ssfile;
${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest_fail basica-nonexist "${testcvs} -q ci nonexist" \
"${PROG}"' [a-z]*: nothing known about `nonexist'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
	  dotest basica-8 "${testcvs} -q update" ''

	  # Test the -f option to ci
	  cd sdir/ssdir
	  dotest basica-8a0 "${testcvs} -q ci -m not-modified ssfile" ''
	  dotest basica-8a "${testcvs} -q ci -f -m force-it" \
"Checking in ssfile;
${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest basica-8a1 "${testcvs} -q ci -m bump-it -r 2.0" \
"Checking in ssfile;
${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.0; previous revision: 1\.3
done"
	  # -f should not be necessary, but it should be harmless.
	  # Also test the "-r 3" (rather than "-r 3.0") usage.
	  dotest basica-8a2 "${testcvs} -q ci -m bump-it -f -r 3" \
"Checking in ssfile;
${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 3\.1; previous revision: 2\.0
done"
	  cd ../..
	  dotest basica-8b "${testcvs} -q diff -r1.2 -r1.3" \
"Index: sdir/ssdir/ssfile
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.2
retrieving revision 1\.3
diff -r1\.2 -r1\.3"

	  # The .* here will normally be "No such file or directory",
	  # but if memory serves some systems (AIX?) have a different message.
:	  dotest_fail basica-9 \
	    "${testcvs} -q -d ${TESTDIR}/nonexist update" \
"${PROG}: cannot access cvs root ${TESTDIR}/nonexist: .*"
	  dotest_fail basica-9 \
	    "${testcvs} -q -d ${TESTDIR}/nonexist update" \
"${PROG} \[[a-z]* aborted\]: ${TESTDIR}/nonexist/CVSROOT: .*"

	  dotest basica-10 "${testcvs} annotate" \
'Annotations for sdir/ssdir/ssfile
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          .'"${username}"' *[0-9a-zA-Z-]*.: ssfile
1\.2          .'"${username}"' *[0-9a-zA-Z-]*.: ssfile line 2'
	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	basicb)
	  # More basic tests, including non-branch tags and co -d.
	  mkdir 1; cd 1
	  dotest basicb-0a "${testcvs} -q co -l ." ''
	  touch topfile
	  dotest basicb-0b "${testcvs} add topfile" \
"${PROG} [a-z]*: scheduling file .topfile. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-0c "${testcvs} -q ci -m add-it topfile" \
"RCS file: ${TESTDIR}/cvsroot/\./topfile,v
done
Checking in topfile;
${TESTDIR}/cvsroot/\./topfile,v  <--  topfile
initial revision: 1\.1
done"
	  cd ..
	  rm -r 1
	  mkdir 2; cd 2
	  dotest basicb-0d "${testcvs} -q co -l ." "U topfile"
	  mkdir first-dir
	  dotest basicb-0e "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd ..
	  rm -r 2

:	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest basicb-1 "${testcvs} -q co first-dir" ''
	  dotest basicb-1a "test -d CVS" ''
	  # See comment at modules3-7f for more on this behavior.
	  dotest basicb-1b "cat CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir" "${TESTDIR}/cvsroot/\."
	  dotest basicb-1c "cat first-dir/CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir"

	  cd first-dir
	  mkdir sdir1 sdir2
	  dotest basicb-2 "${testcvs} add sdir1 sdir2" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir1 added to the repository
Directory ${TESTDIR}/cvsroot/first-dir/sdir2 added to the repository"
	  cd sdir1
	  echo sfile1 starts >sfile1
	  dotest basicb-2a10 "${testcvs} -n add sfile1" \
"${PROG} [a-z]*: scheduling file .sfile1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-2a11 "${testcvs} status sfile1" \
"${PROG} [a-z]*: use .cvs add' to create an entry for sfile1
===================================================================
File: sfile1           	Status: Unknown

   Working revision:	No entry for sfile1
   Repository revision:	No revision control file"
	  dotest basicb-3 "${testcvs} add sfile1" \
"${PROG} [a-z]*: scheduling file .sfile1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-3a1 "${testcvs} status sfile1" \
"===================================================================
File: sfile1           	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  cd ../sdir2
	  echo sfile2 starts >sfile2
	  dotest basicb-4 "${testcvs} add sfile2" \
"${PROG} [a-z]*: scheduling file .sfile2. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-4a "${testcvs} -q ci CVS" \
"${PROG} [a-z]*: warning: directory CVS specified in argument
${PROG} [a-z]*: but CVS uses CVS for its own purposes; skipping CVS directory"
	  cd ..
	  dotest basicb-5 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir1/sfile1,v
done
Checking in sdir1/sfile1;
${TESTDIR}/cvsroot/first-dir/sdir1/sfile1,v  <--  sfile1
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir2/sfile2,v
done
Checking in sdir2/sfile2;
${TESTDIR}/cvsroot/first-dir/sdir2/sfile2,v  <--  sfile2
initial revision: 1\.1
done"
	  echo sfile1 develops >sdir1/sfile1
	  dotest basicb-6 "${testcvs} -q ci -m modify" \
"Checking in sdir1/sfile1;
${TESTDIR}/cvsroot/first-dir/sdir1/sfile1,v  <--  sfile1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest basicb-7 "${testcvs} -q tag release-1" 'T sdir1/sfile1
T sdir2/sfile2'
	  echo not in time for release-1 >sdir2/sfile2
	  dotest basicb-8 "${testcvs} -q ci -m modify-2" \
"Checking in sdir2/sfile2;
${TESTDIR}/cvsroot/first-dir/sdir2/sfile2,v  <--  sfile2
new revision: 1\.2; previous revision: 1\.1
done"
	  # See if CVS can correctly notice when an invalid numeric
	  # revision is specified.
	  # Commented out until we get around to fixing CVS
:	  dotest basicb-8a0 "${testcvs} diff -r 1.5 -r 1.7 sfile2" 'error msg'
	  cd ..

	  # Test that we recurse into the correct directory when checking
	  # for existing files, even if co -d is in use.
	  touch first-dir/extra
	  dotest basicb-cod-1 "${testcvs} -q co -d first-dir1 first-dir" \
'U first-dir1/sdir1/sfile1
U first-dir1/sdir2/sfile2'
	  rm -r first-dir1

	  rm -r first-dir
	  dotest basicb-9 \
"${testcvs} -q co -d newdir -r release-1 first-dir/sdir1 first-dir/sdir2" \
'U newdir/sdir1/sfile1
U newdir/sdir2/sfile2'
	  dotest basicb-9a "test -d CVS" ''
	  # See comment at modules3-7f for more on this behavior.
	  dotest basicb-9b "cat CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir" "${TESTDIR}/cvsroot/\."
	  dotest basicb-9c "cat newdir/CVS/Repository" \
"${TESTDIR}/cvsroot/CVSROOT/Emptydir"
	  dotest basicb-10 "cat newdir/sdir1/sfile1 newdir/sdir2/sfile2" \
"sfile1 develops
sfile2 starts"

	  rm -r newdir

	  # Hmm, this might be a case for CVSNULLREPOS, but CVS doesn't
	  # seem to deal with it...
	  if false; then
	  dotest basicb-11 "${testcvs} -q co -d sub1/sub2 first-dir" \
"U sub1/sub2/sdir1/sfile1
U sub1/sub2/sdir2/sfile2"
	  cd sub1
	  dotest basicb-12 "${testcvs} -q update" ''
	  touch xx
	  dotest basicb-13 "${testcvs} add xx" fixme
	  cd ..
	  rm -r sub1
	  # to test: sub1/sub2/sub3
	  fi # end of tests commented out.

	  # Create a second directory.
	  mkdir 1
	  cd 1
	  dotest basicb-14 "${testcvs} -q co -l ." 'U topfile'
	  mkdir second-dir
	  dotest basicb-15 "${testcvs} add second-dir" \
"Directory ${TESTDIR}/cvsroot/second-dir added to the repository"
	  cd second-dir
	  touch aa
	  dotest basicb-16 "${testcvs} add aa" \
"${PROG} [a-z]*: scheduling file .aa. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-17 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/second-dir/aa,v
done
Checking in aa;
${TESTDIR}/cvsroot/second-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1
	  # Now here is the kicker: note that the semantics of -d
	  # are fundamentally different if we specify two or more directories 
	  # rather than one!  I consider this to be seriously bogus,
	  # but for the moment I am just trying to figure out what
	  # CVS's current behaviors are.
	  dotest basicb-18 "${testcvs} -q co -d test2 first-dir second-dir" \
"U test2/first-dir/sdir1/sfile1
U test2/first-dir/sdir2/sfile2
U test2/second-dir/aa"
	  cd test2
	  touch emptyfile
	  # The fact that CVS lets us add a file here is a CVS bug, right?
	  # I can just make this an error message (on the add and/or the
	  # commit) without getting flamed, right?
	  # Right?
	  # Right?
	  dotest basicb-19 "${testcvs} add emptyfile" \
"${PROG} [a-z]*: scheduling file .emptyfile. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest basicb-20 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/CVSROOT/Emptydir/emptyfile,v
done
Checking in emptyfile;
${TESTDIR}/cvsroot/CVSROOT/Emptydir/emptyfile,v  <--  emptyfile
initial revision: 1\.1
done"
	  cd ..

	  mkdir 1; cd 1
	  # "cvs admin" tests are scattered around a bit.  Here we test
	  # ability to reject an unrecognized option.  The "keyword"
	  # test has a test of "cvs admin -l" and the "binfiles" test
	  # has a test of "cvs admin -k".  Note that -H is an illegal
	  # option.  It probably should be an error message.  But 
	  # currently it is one error message for each file operated on,
	  # which in this case is zero files.
	  dotest basicb-21 "${testcvs} -q admin -H" ""
	  cd ..
	  rmdir 1

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r test2

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -rf ${CVSROOT_DIRNAME}/second-dir
	  rm -rf ${CVSROOT_DIRNAME}/CVSROOT/Emptydir
	  rm -f ${CVSROOT_DIRNAME}/topfile,v
	  ;;

	basic1)
	  # first dive - add a files, first singly, then in a group.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  # check out an empty directory
	  dotest basic1-1 "${testcvs} -q co first-dir" ''

	  cd first-dir
	  echo file2 >file2
	  echo file3 >file3
	  echo file4 >file4
	  echo file5 >file5

	  dotest basic1-14-add-add "${testcvs} add file2 file3 file4 file5" \
"${PROG} [a-z]*: scheduling file \`file2' for addition
${PROG} [a-z]*: scheduling file \`file3' for addition
${PROG} [a-z]*: scheduling file \`file4' for addition
${PROG} [a-z]*: scheduling file \`file5' for addition
${PROG} [a-z]*: use 'cvs commit' to add these files permanently"
	  dotest basic1-15-add-add \
"${testcvs} -q update file2 file3 file4 file5" \
"A file2
A file3
A file4
A file5"
	  dotest basic1-16-add-add "${testcvs} -q update" \
"A file2
A file3
A file4
A file5"
	  dotest basic1-17-add-add "${testcvs} -q status" \
"===================================================================
File: file2            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file4            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file5            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest basic1-18-add-add "${testcvs} -q log" \
"${PROG} [a-z]*: file2 has been added, but not committed
${PROG} [a-z]*: file3 has been added, but not committed
${PROG} [a-z]*: file4 has been added, but not committed
${PROG} [a-z]*: file5 has been added, but not committed"
	  cd ..
	  dotest basic1-21-add-add "${testcvs} -q update" \
"A first-dir/file2
A first-dir/file3
A first-dir/file4
A first-dir/file5"
	  # FIXCVS?  Shouldn't this read first-dir/file2 instead of file2?
	  dotest basic1-22-add-add "${testcvs} log first-dir" \
"${PROG} [a-z]*: Logging first-dir
${PROG} [a-z]*: file2 has been added, but not committed
${PROG} [a-z]*: file3 has been added, but not committed
${PROG} [a-z]*: file4 has been added, but not committed
${PROG} [a-z]*: file5 has been added, but not committed"
	  dotest basic1-23-add-add "${testcvs} status first-dir" \
"${PROG} [a-z]*: Examining first-dir
===================================================================
File: file2            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file4            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file5            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest basic1-24-add-add "${testcvs} update first-dir" \
"${PROG} [a-z]*: Updating first-dir
A first-dir/file2
A first-dir/file3
A first-dir/file4
A first-dir/file5"
	  dotest basic1-27-add-add "${testcvs} co first-dir" \
"${PROG} [a-z]*: Updating first-dir
A first-dir/file2
A first-dir/file3
A first-dir/file4
A first-dir/file5"
	  cd first-dir
	  dotest basic1-14-add-ci \
"${testcvs} commit -m test file2 file3 file4 file5" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file3,v
done
Checking in file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file5,v
done
Checking in file5;
${TESTDIR}/cvsroot/first-dir/file5,v  <--  file5
initial revision: 1\.1
done"
	  dotest basic1-15-add-ci \
"${testcvs} -q update file2 file3 file4 file5" ''
	  dotest basic1-16-add-ci "${testcvs} -q update" ''
	  dotest basic1-17-add-ci "${testcvs} -q status" \
"===================================================================
File: file2            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file3,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file4            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file4,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file5            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file5,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  # The "log" tests and friends probably already test the output 
	  # from log quite adequately.
	  # Note: using dotest fails here.  It seems to be related
	  # to the output being sufficiently large (Red Hat 4.1).
	  # dotest basic1-18-add-ci "${testcvs} log" "${DOTSTAR}"
	  if ${testcvs} -q log >>${LOGFILE}; then
	    pass basic1-18-add-ci
	  else
	    pass basic1-18-add-ci
	  fi
	  cd ..
	  dotest basic1-21-add-ci "${testcvs} -q update" ''
	  # See test basic1-18-add-ci for explanation of non-use of dotest.
	  if ${testcvs} -q log first-dir >>${LOGFILE}; then
	    pass basic1-22-add-ci
	  else
	    pass basic1-22-add-ci
	  fi
	  # At least for the moment I am going to consider 17-add-ci
	  # an adequate test of the output here.
	  # See test basic1-18-add-ci for explanation of non-use of dotest.
	  if ${testcvs} -q status first-dir >>${LOGFILE}; then
	    pass basic1-23-add-ci
	  else
	    pass basic1-23-add-ci
	  fi
	  dotest basic1-24-add-ci "${testcvs} -q update first-dir" ''
	  dotest basic1-27-add-ci "${testcvs} -q co first-dir" ''

	  cd first-dir
	  rm file2 file3 file4 file5
	  dotest basic1-14-rm-rm "${testcvs} rm file2 file3 file4 file5" \
"${PROG} [a-z]*: scheduling .file2. for removal
${PROG} [a-z]*: scheduling .file3. for removal
${PROG} [a-z]*: scheduling .file4. for removal
${PROG} [a-z]*: scheduling .file5. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove these files permanently"
	  # 15-rm-rm was commented out.  Why?
	  dotest basic1-15-rm-rm \
"${testcvs} -q update file2 file3 file4 file5" \
"R file2
R file3
R file4
R file5"
	  dotest basic1-16-rm-rm "${testcvs} -q update" \
"R file2
R file3
R file4
R file5"
	  dotest basic1-17-rm-rm "${testcvs} -q status" \
"===================================================================
File: no file file2		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file3		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file3,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file4		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file4,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file5		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file5,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  # Would be nice to test that real logs appear (with dead state
	  # and all), either here or someplace like log2 tests.
	  if ${testcvs} -q log >>${LOGFILE}; then
	    pass basic1-18-rm-rm
	  else
	    fail basic1-18-rm-rm
	  fi
	  cd ..
	  dotest basic1-21-rm-rm "${testcvs} -q update" \
"R first-dir/file2
R first-dir/file3
R first-dir/file4
R first-dir/file5"
	  if ${testcvs} -q log first-dir >>${LOGFILE}; then
	    pass basic1-22-rm-rm
	  else
	    fail basic1-22-rm-rm
	  fi
	  if ${testcvs} -q status first-dir >>${LOGFILE}; then
	    pass basic1-23-rm-rm
	  else
	    fail basic1-23-rm-rm
	  fi
	  dotest basic1-24-rm-rm "${testcvs} -q update first-dir" \
"R first-dir/file2
R first-dir/file3
R first-dir/file4
R first-dir/file5"
	  dotest basic1-27-rm-rm "${testcvs} -q co first-dir" \
"R first-dir/file2
R first-dir/file3
R first-dir/file4
R first-dir/file5"
	  cd first-dir
	  dotest basic1-14-rm-ci "${testcvs} -q commit -m test" \
"Removing file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done
Removing file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
new revision: delete; previous revision: 1\.1
done
Removing file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1
done
Removing file5;
${TESTDIR}/cvsroot/first-dir/file5,v  <--  file5
new revision: delete; previous revision: 1\.1
done"
	  dotest basic1-15-rm-ci \
"${testcvs} -q update file2 file3 file4 file5" ''
	  dotest basic1-16-rm-ci "${testcvs} -q update" ''
	  dotest basic1-17-rm-ci "${testcvs} -q status" ''
	  # Would be nice to test that real logs appear (with dead state
	  # and all), either here or someplace like log2 tests.
	  if ${testcvs} -q log >>${LOGFILE}; then
	    pass basic1-18-rm-ci
	  else
	    fail basic1-18-rm-ci
	  fi
	  cd ..
	  dotest basic1-21-rm-ci "${testcvs} -q update" ''
	  if ${testcvs} -q log first-dir >>${LOGFILE}; then
	    pass basic1-22-rm-ci
	  else
	    fail basic1-22-rm-ci
	  fi
	  if ${testcvs} -q status first-dir >>${LOGFILE}; then
	    pass basic1-23-rm-ci
	  else
	    fail basic1-23-rm-ci
	  fi
	  dotest basic1-24-rm-ci "${testcvs} -q update first-dir" ''
	  dotest basic1-27-rm-ci "${testcvs} -q co first-dir" ''
	  cd first-dir
	  # All the files are removed, so nothing gets tagged.
	  dotest basic1-28 "${testcvs} -q tag first-dive" ''
	  cd ..
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	deep)
	  # Test the ability to operate on directories nested rather deeply.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest deep-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  for i in dir1 dir2 dir3 dir4 dir5 dir6 dir7 dir8; do
	    mkdir $i
	    dotest deep-2-$i "${testcvs} add $i" \
"Directory ${TESTDIR}/cvsroot/first-dir/dir1[/dir0-9]* added to the repository"
	    cd $i
	    echo file1 >file1
	    dotest deep-3-$i "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  done
	  cd ../../../../../../../../..
	  dotest_lit deep-4 "${testcvs} -q ci -m add-them first-dir" <<HERE
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/file1,v
done
Checking in first-dir/dir1/file1;
${TESTDIR}/cvsroot/first-dir/dir1/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/file1,v
done
Checking in first-dir/dir1/dir2/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/file1,v
done
Checking in first-dir/dir1/dir2/dir3/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
initial revision: 1.1
done
HERE

	  cd first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8
	  rm file1
	  dotest deep-4a0 "${testcvs} rm file1" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  dotest deep-4a1 "${testcvs} -q ci -m rm-it" "Removing file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  cd ../../..
	  dotest deep-4a2 "${testcvs} -q update -P dir6/dir7" ''
	  # Should be using "test -e" if that is portable enough.
	  dotest_fail deep-4a3 "test -d dir6/dir7/dir8" ''

	  # Test that if we remove the working directory, CVS does not
	  # recreate it.  (I realize that this behavior is what the
	  # users expect, but in the longer run we might want to
	  # re-think it.  The corresponding behavior for a file is that
	  # CVS *will* recreate it, and we might want to make it so
	  # that "cvs release -d" is the way to delete the directory
	  # and have it stay gone -kingdon, Oct1996).
	  rm -r dir6
	  dotest deep-4b1 "${testcvs} -q update" ''
	  dotest deep-4b2 "${testcvs} -q update -d -P" \
'U dir6/file1
U dir6/dir7/file1'

	  # Test what happens if one uses -P when there are files removed
	  # but not committed.
	  cd dir6/dir7
	  dotest deep-rm1 "${testcvs} rm -f file1" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  cd ..
	  dotest deep-rm2 "${testcvs} -q update -d -P" 'R dir7/file1'
	  dotest deep-rm3 "test -d dir7" ''
	  dotest deep-rm4 "${testcvs} -q ci -m rm-it" "Removing dir7/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  dotest deep-rm5 "${testcvs} -q update -d -P" ''
	  dotest_fail deep-rm6 "test -d dir7" ''

	  # Test rm -f -R.
	  cd ../..
	  dotest deep-rm7 "${testcvs} rm -f -R dir5" \
"${PROG} [a-z]*: Removing dir5
${PROG} [a-z]*: scheduling .dir5/file1. for removal
${PROG} [a-z]*: Removing dir5/dir6
${PROG} [a-z]*: scheduling .dir5/dir6/file1. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove these files permanently"
	  dotest deep-rm8 "${testcvs} -q ci -m rm-it" \
"Removing dir5/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done
Removing dir5/dir6/file1;
${TESTDIR}/cvsroot/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  dotest deep-rm9 "${testcvs} -q update -d -P" ''
	  dotest_fail deep-rm10 "test -d dir5"

	  cd ../../../../..

	  if echo "yes" | ${testcvs} release -d first-dir >>${LOGFILE}; then
	    pass deep-5
	  else
	    fail deep-5
	  fi
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	basic2)
		# Test rtag, import, history, various miscellaneous operations

		# NOTE: this section has reached the size and
		# complexity where it is getting to be a good idea to
		# add new tests to a new section rather than
		# continuing to piggyback them onto the tests here.

		# First empty the history file
		rm ${CVSROOT_DIRNAME}/CVSROOT/history
		touch ${CVSROOT_DIRNAME}/CVSROOT/history

### XXX maybe should use 'cvs imprt -b1 -m new-module first-dir F F1' in an
### empty directory to do this instead of hacking directly into $CVSROOT
		mkdir ${CVSROOT_DIRNAME}/first-dir
		dotest basic2-1 "${testcvs} -q co first-dir" ''
		for i in first-dir dir1 dir2 ; do
			if test ! -d $i ; then
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

# XXX why is this commented out???
#		if ${CVS} diff -u first-dir   >> ${LOGFILE} || test $? = 1 ; then
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

		# FIXME: doesn't work right for added files
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

# XXX why is this commented out?
#		if ${CVS} diff -u first-dir  >> ${LOGFILE} || test $? = 1 ; then
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
		if test -d first-dir ; then
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
		if ${CVS} rdiff -r1.1 -rrtagged-by-head first-dir  >> ${LOGFILE} || test $? = 1 ; then
			echo "PASS: test 49" >>${LOGFILE}
		else
			echo "FAIL: test 49" | tee -a ${LOGFILE} ; exit 1
		fi

		# now export by rtagged-by-head and rtagged-by-tag and compare.
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
		rm -r 1dir first-dir

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
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - . | (cd ../first-dir.cpy ; tar xf -))

		directory_cmp first-dir export-dir

		if $ISDIFF ; then
			echo "FAIL: test 55" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 55" >>${LOGFILE}
		fi

		# interrupt, while we've got a clean 1.1 here, let's import it
		# into a couple of other modules.
		cd export-dir
		dotest 56 "${testcvs} import -m first-import second-dir first-immigration immigration1 immigration1_0" \
"N second-dir/file14
N second-dir/file6
N second-dir/file7
${PROG} [a-z]*: Importing ${TESTDIR}/cvsroot/second-dir/dir1
N second-dir/dir1/file14
N second-dir/dir1/file6
N second-dir/dir1/file7
${PROG} [a-z]*: Importing ${TESTDIR}/cvsroot/second-dir/dir1/dir2
N second-dir/dir1/dir2/file14
N second-dir/dir1/dir2/file6
N second-dir/dir1/dir2/file7

No conflicts created by this import"
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

		rm -r second-dir

		rm -r export-dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - . | (cd ../first-dir ; tar xf -))

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
		rm -r 1dir first-dir

		# Test the cvs history command.

		# The reason that there are two patterns rather than using
		# \(${TESTDIR}\|<remote>\) is that we are trying to
		# make this portable.  Perhaps at some point we should
		# ditch that notion and require GNU expr (or dejagnu or....)
		# since it seems to be so painful.

		# why are there two lines at the end of the local output
		# which don't exist in the remote output?  would seem to be
		# a CVS bug.
		dotest basic2-64 "${testcvs} his -x TOFWUCGMAR -a" \
"O [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir           =first-dir= ${TMPPWD}/\*
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir           == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir           == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1      == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1      == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1/dir2 == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1/dir2 == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir           == ${TMPPWD}
M [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir           == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1      == ${TMPPWD}
M [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1      == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1/dir2 == ${TMPPWD}
M [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1/dir2 == ${TMPPWD}
F [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]*                     =first-dir= ${TMPPWD}/\*
T [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-revision:1\.1\]
O [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* \[1\.1\] first-dir           =first-dir= ${TMPPWD}/\*
U [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir           == ${TMPPWD}/first-dir
U [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file7     first-dir           == ${TMPPWD}/first-dir" \
"O [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir           =first-dir= <remote>/\*
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir           == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file6     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file7     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir           == <remote>
M [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1      == <remote>
M [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.1 file14    first-dir/dir1/dir2 == <remote>
M [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* 1\.2 file6     first-dir/dir1/dir2 == <remote>
F [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]*                     =first-dir= <remote>/\*
T [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* first-dir \[rtagged-by-revision:1\.1\]
O [0-9/]* [0-9:]* ${PLUS}0000 [a-z0-9@][a-z0-9@]* \[1\.1\] first-dir           =first-dir= <remote>/\*"

		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf ${CVSROOT_DIRNAME}/second-dir
		;;

	rdiff)
		# Test rdiff
		# XXX for now this is just the most essential test...
		cd ${TESTDIR}

		mkdir testimport
		cd testimport
		echo '$''Id$' > foo
		echo '$''Name$' >> foo
		echo '$''Id$' > bar
		echo '$''Name$' >> bar
		dotest rdiff-1 \
		  "${testcvs} import -I ! -m test-import-with-keyword trdiff TRDIFF T1" \
'N trdiff/foo
N trdiff/bar

No conflicts created by this import'
		dotest rdiff-2 \
		  "${testcvs} co -ko trdiff" \
"${PROG} [a-z]*: Updating trdiff
U trdiff/bar
U trdiff/foo"
		cd trdiff
		echo something >> foo
		dotest rdiff-3 \
		  "${testcvs} ci -m added-something foo" \
"Checking in foo;
${TESTDIR}/cvsroot/trdiff/foo,v  <--  foo
new revision: 1\.2; previous revision: 1\.1
done"
		echo '#ident	"@(#)trdiff:$''Name$:$''Id$"' > new
		echo "new file" >> new
		dotest rdiff-4 \
		  "${testcvs} add -m new-file-description new" \
"${PROG} [a-z]*: scheduling file \`new' for addition
${PROG} [a-z]*: use 'cvs commit' to add this file permanently"
		dotest rdiff-5 \
		  "${testcvs} commit -m added-new-file new" \
"RCS file: ${TESTDIR}/cvsroot/trdiff/new,v
done
Checking in new;
${TESTDIR}/cvsroot/trdiff/new,v  <--  new
initial revision: 1\.1
done"
		dotest rdiff-6 \
		  "${testcvs} tag local-v0" \
"${PROG} [a-z]*: Tagging .
T bar
T foo
T new"
		dotest rdiff-7 \
		  "${testcvs} status -v foo" \
"===================================================================
File: foo              	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${TESTDIR}/cvsroot/trdiff/foo,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-ko

   Existing Tags:
	local-v0                 	(revision: 1\.2)
	T1                       	(revision: 1\.1\.1\.1)
	TRDIFF                   	(branch: 1\.1\.1)"

		cd ..
		rm -r trdiff

		dotest rdiff-8 \
		  "${testcvs} rdiff -r T1 -r local-v0 trdiff" \
"${PROG}"' [a-z]*: Diffing trdiff
Index: trdiff/foo
diff -c trdiff/foo:1\.1\.1\.1 trdiff/foo:1\.2
\*\*\* trdiff/foo:1\.1\.1\.1	.*
--- trdiff/foo	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,2 \*\*\*\*
! \$''Id: foo,v 1\.1\.1\.1 [0-9/]* [0-9:]* '"${username}"' Exp \$
! \$''Name: T1 \$
--- 1,3 ----
! \$''Id: foo,v 1\.2 [0-9/]* [0-9:]* '"${username}"' Exp \$
! \$''Name: local-v0 \$
! something
Index: trdiff/new
diff -c /dev/null trdiff/new:1\.1
\*\*\* /dev/null	.*
--- trdiff/new	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1,2 ----
'"${PLUS}"' #ident	"@(#)trdiff:\$''Name: local-v0 \$:\$''Id: new,v 1\.1 [0-9/]* [0-9:]* '"${username}"' Exp \$"
'"${PLUS}"' new file'

# FIXME: will this work here?
#		if test "$keep" = yes; then
#		  echo Keeping ${TESTDIR} and exiting due to --keep
#		  exit 0
#		fi

		cd ..
		rm -r testimport
		rm -rf ${CVSROOT_DIRNAME}/trdiff
		;;

	death)
		# next dive.  test death support.

		# NOTE: this section has reached the size and
		# complexity where it is getting to be a good idea to
		# add new death support tests to a new section rather
		# than continuing to piggyback them onto the tests here.

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
"Directory ${TESTDIR}/cvsroot/first-dir/subdir added to the repository"
		cd subdir
		echo file in subdir >sfile
		dotest 65a1 "${testcvs} add sfile" \
"${PROG}"' [a-z]*: scheduling file `sfile'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
		dotest 65a2 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/subdir/sfile,v
done
Checking in sfile;
${TESTDIR}/cvsroot/first-dir/subdir/sfile,v  <--  sfile
initial revision: 1\.1
done"
		rm sfile
		dotest 65a3 "${testcvs} rm sfile" \
"${PROG}"' [a-z]*: scheduling `sfile'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG}"' commit'\'' to remove this file permanently'
		dotest 65a4 "${testcvs} -q ci -m remove-it" \
"Removing sfile;
${TESTDIR}/cvsroot/first-dir/subdir/sfile,v  <--  sfile
new revision: delete; previous revision: 1\.1
done"
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
"RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1\.1
done"
		rm file4
		dotest death-file4-rm "${testcvs} remove file4" \
"${PROG}"' [a-z]*: scheduling `file4'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG}"' commit'\'' to remove this file permanently'
		dotest death-file4-cirm "${testcvs} -q ci -m remove file4" \
"Removing file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1
done"

		# Tag the branchpoint.
		dotest death-72a "${testcvs} -q tag bp_branch1" 'T file1
T file2'

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

		dotest death-76a0 \
"${testcvs} -q rdiff -r bp_branch1 -r branch1 first-dir" \
"Index: first-dir/file3
diff -c /dev/null first-dir/file3:1\.1\.2\.1
\*\*\* /dev/null	.*
--- first-dir/file3	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} line1 from branch1"
		dotest death-76a1 \
"${testcvs} -q rdiff -r branch1 -r bp_branch1 first-dir" \
'Index: first-dir/file3
diff -c first-dir/file3:1\.1\.2\.1 first-dir/file3:removed
\*\*\* first-dir/file3:1\.1\.2\.1	.*
--- first-dir/file3	.*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- line1 from branch1
--- 0 ----'

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

		if test -f file3 ; then
			echo "FAIL: test 85" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 85" >>${LOGFILE}
		fi

		# join
		dotest 86 "${testcvs} -q update -j branch1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.3\.2\.1
Merging differences between 1\.3 and 1\.3\.2\.1 into file1
${PROG} [a-z]*: scheduling file2 for removal
U file3"

		dotest_fail death-file4-5 "test -f file4" ''

		if test -f file3 ; then
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
		dotest 89 "${testcvs} -q ci -m test" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done
Removing file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done
Checking in file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
new revision: 1\.2; previous revision: 1\.1
done"
		cd ..
		mkdir 2
		cd 2
		dotest 89a "${testcvs} -q co first-dir" 'U first-dir/file1
U first-dir/file3'
		cd ..
		rm -r 2
		cd first-dir

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

		if test -f file1 ; then
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
		if test -f file3 ; then
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

		if test -f file1 ; then
			echo "PASS: test 94" >>${LOGFILE}
		else
			echo "FAIL: test 94" | tee -a ${LOGFILE} ; exit 1
		fi

		# and join
		dotest 95 "${testcvs} -q update -j HEAD" \
"${PROG}"' [a-z]*: file file1 has been modified, but has been removed in revision HEAD
'"${PROG}"' [a-z]*: file file3 exists, but has been added in revision HEAD'

		dotest_fail death-file4-7 "test -f file4" ''

		# file2 should not have been recreated.  It was
		# deleted on the branch, and has not been modified on
		# the trunk.  That means that there have been no
		# changes between the greatest common ancestor (the
		# trunk version) and HEAD.
		dotest_fail death-file2-1 "test -f file2" ''

		cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
		;;

	death2)
	  # More tests of death support.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest death2-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add a file on the trunk.
	  echo "first revision" > file1
	  dotest death2-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  dotest death2-3 "${testcvs} -q commit -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  # Make a branch and a non-branch tag.
	  dotest death2-4 "${testcvs} -q tag -b branch" 'T file1'
	  dotest death2-5 "${testcvs} -q tag tag" 'T file1'

	  # Switch over to the branch.
	  dotest death2-6 "${testcvs} -q update -r branch" ''

	  # Delete the file on the branch.
	  rm file1
	  dotest death2-7 "${testcvs} rm file1" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"

	  # Test diff of the removed file before it is committed.
	  dotest_fail death2-diff-1 "${testcvs} -q diff file1" \
"${PROG} [a-z]*: file1 was removed, no comparison available"

	  dotest_fail death2-diff-2 "${testcvs} -q diff -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* ${tempname}[ 	][	]*[a-zA-Z0-9: ]*
--- /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  dotest death2-8 "${testcvs} -q ci -m removed" \
"Removing file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1\.2
done"

	  # Test diff of a dead file.
	  dotest_fail death2-diff-3 \
"${testcvs} -q diff -r1.1 -rbranch -c file1" \
"${PROG} [a-z]*: file1 was removed, no comparison available"

	  dotest_fail death2-diff-4 \
"${testcvs} -q diff -r1.1 -rbranch -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* ${tempname}[ 	][	]*[a-zA-Z0-9: ]*
--- /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  dotest_fail death2-diff-5 "${testcvs} -q diff -rtag -c ." \
"${PROG} [a-z]*: file1 no longer exists, no comparison available"

	  dotest_fail death2-diff-6 "${testcvs} -q diff -rtag -N -c ." \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* ${tempname}[ 	][	]*[a-zA-Z0-9: ]*
--- /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  # Test rdiff of a dead file.
	  dotest death2-rdiff-1 \
"${testcvs} -q rtag -rbranch rdiff-tag first-dir" ''

	  dotest death2-rdiff-2 "${testcvs} -q rdiff -rtag -rbranch first-dir" \
"Index: first-dir/file1
diff -c first-dir/file1:1\.1 first-dir/file1:removed
\*\*\* first-dir/file1:1\.1[ 	][	]*[a-zA-Z0-9: ]*
--- first-dir/file1[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  # Readd the file to the branch.
	  echo "second revision" > file1
	  dotest death2-9 "${testcvs} add file1" \
"${PROG}"' [a-z]*: file `file1'\'' will be added on branch `branch'\'' from version 1\.1\.2\.1
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  # Test diff of the added file before it is committed.
	  dotest_fail death2-diff-7 "${testcvs} -q diff file1" \
"${PROG} [a-z]*: file1 is a new entry, no comparison available"

	  dotest_fail death2-diff-8 "${testcvs} -q diff -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- ${tempname}[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} second revision"

	  dotest death2-10 "${testcvs} -q commit -m add" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # Back to the trunk.
	  dotest death2-11 "${testcvs} -q update -A" 'U file1' 'P file1'

	  # Add another file on the trunk.
	  echo "first revision" > file2
	  dotest death2-12 "${testcvs} add file2" \
"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-13 "${testcvs} -q commit -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  # Back to the branch.
	  # The ``no longer in the repository'' message doesn't really
	  # look right to me, but that's what CVS currently prints for
	  # this case.
	  dotest death2-14 "${testcvs} -q update -r branch" \
"U file1
${PROG} [a-z]*: file2 is no longer in the repository" \
"P file1
${PROG} [a-z]*: file2 is no longer in the repository"

	  # Add a file on the branch with the same name.
	  echo "branch revision" > file2
	  dotest death2-15 "${testcvs} add file2" \
"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-16 "${testcvs} -q commit -m add" \
"Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Add a new file on the branch.
	  echo "first revision" > file3
	  dotest death2-17 "${testcvs} add file3" \
"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest death2-18 "${testcvs} -q commit -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/file3,v
done
Checking in file3;
${TESTDIR}/cvsroot/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Test diff of a nonexistent tag
	  dotest_fail death2-diff-9 "${testcvs} -q diff -rtag -c file3" \
"${PROG} [a-z]*: tag tag is not in file file3"

	  dotest_fail death2-diff-10 "${testcvs} -q diff -rtag -N -c file3" \
"Index: file3
===================================================================
RCS file: file3
diff -N file3
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- ${tempname}[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  dotest_fail death2-diff-11 "${testcvs} -q diff -rtag -c ." \
"Index: file1
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
diff -c -r1\.1 -r1\.1\.2\.2
\*\*\* file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
--- file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! first revision
--- 1 ----
! second revision
${PROG} [a-z]*: tag tag is not in file file2
${PROG} [a-z]*: tag tag is not in file file3"

	  dotest_fail death2-diff-12 "${testcvs} -q diff -rtag -c -N ." \
"Index: file1
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
diff -c -r1\.1 -r1\.1\.2\.2
\*\*\* file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
--- file1[ 	][ 	]*[a-zA-Z0-9:./ 	]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! first revision
--- 1 ----
! second revision
Index: file2
===================================================================
RCS file: file2
diff -N file2
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- ${tempname}[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} branch revision
Index: file3
===================================================================
RCS file: file3
diff -N file3
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- ${tempname}[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  # Switch to the nonbranch tag.
	  dotest death2-19 "${testcvs} -q update -r tag" \
"U file1
${PROG} [a-z]*: file2 is no longer in the repository
${PROG} [a-z]*: file3 is no longer in the repository" \
"P file1
${PROG} [a-z]*: file2 is no longer in the repository
${PROG} [a-z]*: file3 is no longer in the repository"

	  dotest_fail death2-20 "test -f file2"

	  # Make sure diff only reports appropriate files.
	  dotest_fail death2-diff-13 "${testcvs} -q diff -r rdiff-tag" \
"${PROG} [a-z]*: file1 is a new entry, no comparison available"

	  dotest_fail death2-diff-14 "${testcvs} -q diff -r rdiff-tag -c -N" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* /dev/null[ 	][ 	]*[a-zA-Z0-9: ]*
--- ${tempname}[ 	][ 	]*[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

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
	  dotest_lit branches-3 "${testcvs} -q ci -m add-it" <<HERE
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file3,v
done
Checking in file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
initial revision: 1.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1.1
done
HERE
	  echo 4:trunk-2 >file4
	  dotest branches-3.2 "${testcvs} -q ci -m trunk-before-branch" \
"Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done"
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
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"
	  dotest branches-7 "${testcvs} -q tag -b brbr" 'T file1
T file2
T file3
T file4'
	  dotest branches-8 "${testcvs} -q update -r brbr" ''
	  echo 1:brbr >file1
	  echo 4:brbr >file4
	  dotest branches-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1\.2\.1; previous revision: 1\.1\.2\.1
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.1\.2\.1; previous revision: 1\.2\.2\.1
done"
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
"Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done"
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
"Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest branches-14.3 "${testcvs} log file4" \
"
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
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
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: ${PLUS}1 -1
trunk-change-after-branch
----------------------------
revision 1\.2
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: ${PLUS}1 -1
branches:  1\.2\.2;
trunk-before-branch
----------------------------
revision 1\.1
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;
add-it
----------------------------
revision 1\.2\.2\.2
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: ${PLUS}1 -1
change-on-br1
----------------------------
revision 1\.2\.2\.1
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: ${PLUS}1 -1
branches:  1\.2\.2\.1\.2;
modify
----------------------------
revision 1\.2\.2\.1\.2\.1
date: [0-9/: ]*;  author: [a-z0-9@][a-z0-9@]*;  state: Exp;  lines: ${PLUS}1 -1
modify
============================================================================="
	  dotest_status branches-14.4 1 \
	    "${testcvs} diff -c -r 1.1 -r 1.3 file4" \
"Index: file4
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.3
diff -c -r1\.1 -r1\.3
\*\*\* file4	[0-9/]* [0-9:]*	1\.1
--- file4	[0-9/]* [0-9:]*	1\.3
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:trunk-3"
	  dotest_status branches-14.5 1 \
	    "${testcvs} diff -c -r 1.1 -r 1.2.2.1 file4" \
"Index: file4
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.2\.2\.1
diff -c -r1\.1 -r1\.2\.2\.1
\*\*\* file4	[0-9/]* [0-9:]*	1\.1
--- file4	[0-9/]* [0-9:]*	1\.2\.2\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:br1"
	  dotest branches-15 \
	    "${testcvs} update -j 1.1.2.1 -j 1.1.2.1.2.1 file1" \
	    "RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1\.2\.1
retrieving revision 1\.1\.2\.1\.2\.1
Merging differences between 1\.1\.2\.1 and 1\.1\.2\.1\.2\.1 into file1
rcsmerge: warning: conflicts during merge"
	  dotest branches-16 "cat file1" '<<<<<<< file1
1:ancest
=======
1:brbr
[>]>>>>>> 1\.1\.2\.1\.2\.1'
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	multibranch)
	  # Test the ability to have several branchpoints coming off the
	  # same revision.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest multibranch-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 1:trunk-1 >file1
	  dotest multibranch-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest_lit multibranch-3 "${testcvs} -q ci -m add-it" <<HERE
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1.1
done
HERE
	  dotest multibranch-4 "${testcvs} tag -b br1" \
"${PROG} [a-z]*: Tagging \.
T file1"
	  dotest multibranch-5 "${testcvs} tag -b br2" \
"${PROG} [a-z]*: Tagging \.
T file1"
	  dotest multibranch-6 "${testcvs} -q update -r br1" ''
	  echo on-br1 >file1
	  dotest multibranch-7 "${testcvs} -q ci -m modify-on-br1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest multibranch-8 "${testcvs} -q update -r br2" '[UP] file1'
	  echo br2 adds a line >>file1
	  dotest multibranch-9 "${testcvs} -q ci -m modify-on-br2" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"
	  dotest multibranch-10 "${testcvs} -q update -r br1" '[UP] file1'
	  dotest multibranch-11 "cat file1" 'on-br1'
	  dotest multibranch-12 "${testcvs} -q update -r br2" '[UP] file1'
	  dotest multibranch-13 "cat file1" '1:trunk-1
br2 adds a line'

	  dotest multibranch-14 "${testcvs} log file1" \
"
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	br2: 1\.1\.0\.4
	br1: 1\.1\.0\.2
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;  1\.1\.4;
add-it
----------------------------
revision 1\.1\.4\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
modify-on-br2
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
modify-on-br1
============================================================================="
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	import) # test death after import
		# import
		mkdir import-dir ; cd import-dir

		for i in 1 2 3 4 ; do
			echo imported file"$i" > imported-f"$i"
		done

		# This directory should be on the default ignore list,
		# so it shouldn't get imported.
		mkdir RCS
		echo ignore.me >RCS/ignore.me

		echo 'import should not expand $''Id$' >>imported-f2
		cp imported-f2 ../imported-f2-orig.tmp

		if ${CVS} import -m first-import first-dir vendor-branch junk-1_0  ; then
			echo "PASS: test 96" >>${LOGFILE}
		else
			echo "FAIL: test 96" | tee -a ${LOGFILE} ; exit 1
		fi

		if cmp ../imported-f2-orig.tmp imported-f2; then
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
			if test -f imported-f"$i" ; then
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
		rm imported-f1
		if ${CVS} rm imported-f1  2>> ${LOGFILE}; then
			echo "PASS: test 99" >>${LOGFILE}
		else
			echo "FAIL: test 99" | tee -a ${LOGFILE} ; exit 1
		fi

		# change
		echo local-change >> imported-f2

		# commit
		if ${CVS} ci -m local-changes  >> ${LOGFILE} 2>&1; then
			echo "PASS: test 100" >>${LOGFILE}
		else
			echo "FAIL: test 100" | tee -a ${LOGFILE} ; exit 1
		fi

		# log
		if ${CVS} log imported-f1 | grep '1.1.1.2 (dead)'  ; then
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
		rm imported-f4

		if ${CVS} rm imported-f4  2>> ${LOGFILE}; then
			echo "PASS: test 103" >>${LOGFILE}
		else
			echo "FAIL: test 103" | tee -a ${LOGFILE} ; exit 1
		fi

		# commit
		if ${CVS} ci -m vendor-removed imported-f4 >>${LOGFILE}; then
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
			echo rev 2 of file $i >> imported-f"$i"
		done
		cp imported-f2 ../imported-f2-orig.tmp

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0  ; then
			echo "PASS: test 106" >>${LOGFILE}
		else
			echo "FAIL: test 106" | tee -a ${LOGFILE} ; exit 1
		fi
		if cmp ../imported-f2-orig.tmp imported-f2; then
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

		if test -f imported-f1 ; then
			echo "FAIL: test 108" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 108" >>${LOGFILE}
		fi

		for i in 2 3 ; do
			if test -f imported-f"$i" ; then
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

		if test -f imported-f4 ; then
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

		dotest import-113 \
"${testcvs} -q co -jjunk-1_0 -jjunk-2_0 first-dir" \
"${PROG} [a-z]*: file first-dir/imported-f1 is present in revision junk-2_0
RCS file: ${TESTDIR}/cvsroot/first-dir/imported-f2,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.2
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.2 into imported-f2
rcsmerge: warning: conflicts during merge"

		cd first-dir

		if test -f imported-f1 ; then
			echo "FAIL: test 114" | tee -a ${LOGFILE} ; exit 1
		else
			echo "PASS: test 114" >>${LOGFILE}
		fi

		for i in 2 3 ; do
			if test -f imported-f"$i" ; then
				echo "PASS: test 115-$i" >>${LOGFILE}
			else
				echo "FAIL: test 115-$i" | tee -a ${LOGFILE} ; exit 1
			fi
		done

		dotest import-116 'cat imported-f2' \
'imported file2
[<]<<<<<< imported-f2
import should not expand \$''Id: imported-f2,v 1\.2 [0-9/]* [0-9:]* [a-z0-9@][a-z0-9@]* Exp \$
local-change
[=]======
import should not expand \$''Id: imported-f2,v 1\.1\.1\.2 [0-9/]* [0-9:]* [a-z0-9@][a-z0-9@]* Exp \$
rev 2 of file 2
[>]>>>>>> 1\.1\.1\.2'

		cd ..
		rm -r first-dir
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -r import-dir
		;;

	join)
	  # Test doing joins which involve adding and removing files.
	  # See also binfile2, which does similar things with binary files.

	  # We check merging changes from T1 to T2 into the main line.
	  # Here are the interesting cases I can think of:
	  #   1) File added between T1 and T2, not on main line.
	  #      File should be marked for addition.
	  #   2) File added between T1 and T2, also added on main line.
	  #      Conflict.
	  #   3) File removed between T1 and T2, unchanged on main line.
	  #      File should be marked for removal.
	  #   4) File removed between T1 and T2, modified on main line.
	  #      If mod checked in, file should be marked for removal.
	  #	 If mod still in working directory, conflict.
	  #   5) File removed between T1 and T2, was never on main line.
	  #      Nothing should happen.
	  #   6) File removed between T1 and T2, also removed on main line.
	  #      Nothing should happen.
	  #   7) File added on main line, not added between T1 and T2.
	  #      Nothing should happen.
	  #   8) File removed on main line, not modified between T1 and T2.
	  #      Nothing should happen.

	  # We also check merging changes from a branch into the main
	  # line.  Here are the interesting cases:
	  #   1) File added on branch, not on main line.
	  #      File should be marked for addition.
	  #   2) File added on branch, also added on main line.
	  #      Conflict.
	  #   3) File removed on branch, unchanged on main line.
	  #      File should be marked for removal.
	  #   4) File removed on branch, modified on main line.
	  #      Conflict.
	  #   5) File removed on branch, was never on main line.
	  #      Nothing should happen.
	  #   6) File removed on branch, also removed on main line.
	  #      Nothing should happen.
	  #   7) File added on main line, not added on branch.
	  #      Nothing should happen.
	  #   8) File removed on main line, not modified on branch.
	  #      Nothing should happen.

	  # In the tests below, fileN represents case N in the above
	  # lists.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest join-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add two files.
	  echo 'first revision of file3' > file3
	  echo 'first revision of file4' > file4
	  echo 'first revision of file6' > file6
	  echo 'first revision of file8' > file8
	  dotest join-2 "${testcvs} add file3 file4 file6 file8" \
"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file6'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file8'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'

	  dotest join-3 "${testcvs} -q commit -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file3,v
done
Checking in file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file6,v
done
Checking in file6;
${TESTDIR}/cvsroot/first-dir/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file8,v
done
Checking in file8;
${TESTDIR}/cvsroot/first-dir/file8,v  <--  file8
initial revision: 1\.1
done"

	  # Make a branch.
	  dotest join-4 "${testcvs} -q tag -b branch ." \
'T file3
T file4
T file6
T file8'

	  # Add file2 and file7, modify file4, and remove file6 and file8.
	  echo 'first revision of file2' > file2
	  echo 'second revision of file4' > file4
	  echo 'first revision of file7' > file7
	  rm file6 file8
	  dotest join-5 "${testcvs} add file2 file7" \
"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file7'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest join-6 "${testcvs} rm file6 file8" \
"${PROG}"' [a-z]*: scheduling `file6'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file8'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG} commit"\'' to remove these files permanently'
	  dotest join-7 "${testcvs} -q ci -mx ." \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done
Removing file6;
${TESTDIR}/cvsroot/first-dir/file6,v  <--  file6
new revision: delete; previous revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file7,v
done
Checking in file7;
${TESTDIR}/cvsroot/first-dir/file7,v  <--  file7
initial revision: 1\.1
done
Removing file8;
${TESTDIR}/cvsroot/first-dir/file8,v  <--  file8
new revision: delete; previous revision: 1\.1
done"

	  # Check out the branch.
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest join-8 "${testcvs} -q co -r branch first-dir" \
'U first-dir/file3
U first-dir/file4
U first-dir/file6
U first-dir/file8'

	  cd first-dir

	  # Modify the files on the branch, so that T1 is not an
	  # ancestor of the main line, and add file5
	  echo 'first branch revision of file3' > file3
	  echo 'first branch revision of file4' > file4
	  echo 'first branch revision of file6' > file6
	  echo 'first branch revision of file5' > file5
	  dotest join-9 "${testcvs} add file5" \
"${PROG}"' [a-z]*: scheduling file `file5'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest join-10 "${testcvs} -q ci -mx ." \
"Checking in file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/file5,v
done
Checking in file5;
${TESTDIR}/cvsroot/first-dir/Attic/file5,v  <--  file5
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file6;
${TESTDIR}/cvsroot/first-dir/Attic/file6,v  <--  file6
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Tag the current revisions on the branch.
	  dotest join-11 "${testcvs} -q tag T1 ." \
'T file3
T file4
T file5
T file6
T file8'

	  # Add file1 and file2, and remove the other files.
	  echo 'first branch revision of file1' > file1
	  echo 'first branch revision of file2' > file2
	  rm file3 file4 file5 file6
	  dotest join-12 "${testcvs} add file1 file2" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest join-13 "${testcvs} rm file3 file4 file5 file6" \
"${PROG}"' [a-z]*: scheduling `file3'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file4'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file5'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file6'\'' for removal
'"${PROG}"' [a-z]*: use '\'"${PROG} commit"\'' to remove these files permanently'
	  dotest join-14 "${testcvs} -q ci -mx ." \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/Attic/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Removing file3;
${TESTDIR}/cvsroot/first-dir/file3,v  <--  file3
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file4;
${TESTDIR}/cvsroot/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file5;
${TESTDIR}/cvsroot/first-dir/Attic/file5,v  <--  file5
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file6;
${TESTDIR}/cvsroot/first-dir/Attic/file6,v  <--  file6
new revision: delete; previous revision: 1\.1\.2\.1
done"

	  # Tag the current revisions on the branch.
	  dotest join-15 "${testcvs} -q tag T2 ." \
'T file1
T file2
T file8'

	  # Do a checkout with a merge.
	  cd ../..
	  mkdir 3
	  cd 3
	  dotest join-16 "${testcvs} -q co -jT1 -jT2 first-dir" \
'U first-dir/file1
U first-dir/file2
'"${PROG}"' [a-z]*: file first-dir/file2 exists, but has been added in revision T2
U first-dir/file3
'"${PROG}"' [a-z]*: scheduling first-dir/file3 for removal
U first-dir/file4
'"${PROG}"' [a-z]*: scheduling first-dir/file4 for removal
U first-dir/file7'

	  # Verify that the right changes have been scheduled.
	  cd first-dir
	  dotest join-17 "${testcvs} -q update" \
'A file1
R file3
R file4'

	  # Modify file4 locally, and do an update with a merge.
	  cd ../../1/first-dir
	  echo 'third revision of file4' > file4
	  dotest join-18 "${testcvs} -q update -jT1 -jT2 ." \
'U file1
'"${PROG}"' [a-z]*: file file2 exists, but has been added in revision T2
'"${PROG}"' [a-z]*: scheduling file3 for removal
M file4
'"${PROG}"' [a-z]*: file file4 is locally modified, but has been removed in revision T2'

	  # Verify that the right changes have been scheduled.
	  dotest join-19 "${testcvs} -q update" \
'A file1
R file3
M file4'

	  # Do a checkout with a merge from a single revision.

	  # FIXME: CVS currently gets this wrong.  file2 has been
	  # added on both the branch and the main line, and so should
	  # be regarded as a conflict.  However, given the way that
	  # CVS sets up the RCS file, there is no way to distinguish
	  # this case from the case of file2 having existed before the
	  # branch was made.  This could be fixed by reserving
	  # a revision somewhere, perhaps 1.1, as an always dead
	  # revision which can be used as the source for files added
	  # on branches.
	  cd ../../3
	  rm -r first-dir
	  dotest join-20 "${testcvs} -q co -jbranch first-dir" \
"U first-dir/file1
U first-dir/file2
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file2
U first-dir/file3
${PROG} [a-z]*: scheduling first-dir/file3 for removal
U first-dir/file4
${PROG} [a-z]*: file first-dir/file4 has been modified, but has been removed in revision branch
U first-dir/file7"

	  # Verify that the right changes have been scheduled.
	  # The M file2 line is a bug; see above join-20.
	  cd first-dir
	  dotest join-21 "${testcvs} -q update" \
'A file1
M file2
R file3'

	  # Checkout the main line again.
	  cd ../../1
	  rm -r first-dir
	  dotest join-22 "${testcvs} -q co first-dir" \
'U first-dir/file2
U first-dir/file3
U first-dir/file4
U first-dir/file7'

	  # Modify file4 locally, and do an update with a merge from a
	  # single revision.
	  # The file2 handling is a bug; see above join-20.
	  cd first-dir
	  echo 'third revision of file4' > file4
	  dotest join-23 "${testcvs} -q update -jbranch ." \
"U file1
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file2
${PROG} [a-z]*: scheduling file3 for removal
M file4
${PROG} [a-z]*: file file4 is locally modified, but has been removed in revision branch"

	  # Verify that the right changes have been scheduled.
	  # The M file2 line is a bug; see above join-20
	  dotest join-24 "${testcvs} -q update" \
'A file1
M file2
R file3
M file4'

	  cd ../..
	  rm -r 1 2 3
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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

		cd ..
		rm -r first-dir
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		;;

	newb)
	  # Test removing a file on a branch and then checking it out.

	  # We call this "newb" only because it, like the "new" tests,
	  # has something to do with "no longer pertinent" messages.
	  # Not necessarily the most brilliant nomenclature.

	  # Create file 'a'.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest newb-123a "${testcvs} -q co first-dir" ''
	  cd first-dir
	  touch a
	  dotest newb-123b "${testcvs} add a" \
"${PROG} [a-z]*: scheduling file .a. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest newb-123c "${testcvs} -q ci -m added" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/a,v
done
Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
initial revision: 1\.1
done"

	  # Make a branch.
	  dotest newb-123d "${testcvs} -q tag -b branch" "T a"

	  # Check out the branch.
	  cd ..
	  rm -r first-dir
	  mkdir 1
	  cd 1
	  dotest newb-123e "${testcvs} -q co -r branch first-dir" \
"U first-dir/a"

	  # Remove 'a' on another copy of the branch.
	  cd ..
	  mkdir 2
	  cd 2
	  dotest newb-123f "${testcvs} -q co -r branch first-dir" \
"U first-dir/a"
	  cd first-dir
	  rm a
	  dotest newb-123g "${testcvs} rm a" \
"${PROG} [a-z]*: scheduling .a. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  dotest newb-123h "${testcvs} -q ci -m removed" \
"Removing a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
new revision: delete; previous revision: 1\.1\.2
done"

	  # Check out the file on the branch.  This should report
	  # that the file is not pertinent, but it should not
	  # say anything else.
	  cd ..
	  rm -r first-dir
	  dotest newb-123i "${testcvs} -q co -r branch first-dir/a" \
"${PROG} [a-z]*: warning: first-dir/a is not (any longer) pertinent"

	  # Update the other copy, and make sure that a is removed.
	  cd ../1/first-dir
	  # "Needs Patch" is a rather strange output here.  Something like
	  # "Removed in Repository" would make more sense.
	  # The "Need Checkout" output is what CVS does if configured
	  # --disable-server.
	  dotest newb-123j0 "${testcvs} status a" \
"===================================================================
File: a                	Status: Needs \(Patch\|Checkout\)

   Working revision:	1\.1.*
   Repository revision:	1\.1\.2\.1	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		branch (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest newb-123j "${testcvs} -q update" \
"${PROG} [a-z]*: warning: a is not (any longer) pertinent"

	  if test -f a; then
	    fail newb-123k
	  else
	    pass newb-123k
	  fi

	  cd ../..
	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	conflicts)
		mkdir ${CVSROOT_DIRNAME}/first-dir

		mkdir 1
		cd 1

		dotest conflicts-124 "${testcvs} -q co first-dir" ''

		cd first-dir
		touch a

		dotest conflicts-125 "${testcvs} add a" \
"${PROG} [a-z]*: scheduling file .a. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
		dotest conflicts-126 "${testcvs} -q ci -m added" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/a,v
done
Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
initial revision: 1\.1
done"

		cd ../..
		mkdir 2
		cd 2

		# The need for TMPPWD here is a (minor) CVS bug; the
		# output should use the name of the repository as specified.
		dotest conflicts-126.5 "${testcvs} co -p first-dir" \
"${PROG} [a-z]*: Updating first-dir
===================================================================
Checking out first-dir/a
RCS:  ${TMPPWD}/cvsroot/first-dir/a,v
VERS: 1\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*"
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
"Directory ${TESTDIR}/cvsroot/first-dir/dir1 added to the repository"
		dotest conflicts-128 "${testcvs} -q ci -m changed" \
"Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done"
		cd ../..

		# Similar to conflicts-126.5, but now the file has nonempty
		# contents.
		mkdir 3
		cd 3
		# The need for TMPPWD here is a (minor) CVS bug; the
		# output should use the name of the repository as specified.
		dotest conflicts-128.5 "${testcvs} co -p -l first-dir" \
"${PROG} [a-z]*: Updating first-dir
===================================================================
Checking out first-dir/a
RCS:  ${TMPPWD}/cvsroot/first-dir/a,v
VERS: 1\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
add a line"
		cd ..
		rmdir 3

		# Now go over the to the other working directory and
		# start testing conflicts
		cd 2/first-dir
		echo add a conflicting line >>a
		dotest_fail conflicts-129 "${testcvs} -q ci -m changed" \
"${PROG}"' [a-z]*: Up-to-date check failed for `a'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
		mkdir dir1
		mkdir sdir
		dotest conflicts-status-0 "${testcvs} status a" \
"===================================================================
File: a                	Status: Needs Merge

   Working revision:	1\.1.*
   Repository revision:	1\.2	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest conflicts-130 "${testcvs} -q update" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
${PROG} [a-z]*: conflicts found in a
C a
${QUESTION} dir1
${QUESTION} sdir" \
"${QUESTION} dir1
${QUESTION} sdir
RCS file: ${TESTDIR}/cvsroot/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
${PROG} [a-z]*: conflicts found in a
C a"
		rmdir dir1 sdir

		dotest conflicts-status-1 "${testcvs} status a" \
"===================================================================
File: a                	Status: File had conflicts on merge

   Working revision:	1\.2.*
   Repository revision:	1\.2	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest_fail conflicts-131 "${testcvs} -q ci -m try" \
"${PROG} [a-z]*: file .a. had a conflict and has not been modified
${PROG} \[[a-z]* aborted\]: correct above errors first!"

		echo lame attempt at resolving it >>a
		# Try to check in the file with the conflict markers in it.
		dotest conflicts-status-2 "${testcvs} status a" \
"===================================================================
File: a                	Status: File had conflicts on merge

   Working revision:	1\.2.*
   Repository revision:	1\.2	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest_fail conflicts-132 "${testcvs} -q ci -m try" \
"${PROG} [a-z]*: file .a. still contains conflict indicators
${PROG} \[[a-z]* aborted\]: correct above errors first!"

		echo resolve conflict >a
		dotest conflicts-status-3 "${testcvs} status a" \
"===================================================================
File: a                	Status: File had conflicts on merge

   Working revision:	1\.2.*
   Repository revision:	1\.2	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest conflicts-133 "${testcvs} -q ci -m resolved" \
"Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
new revision: 1\.3; previous revision: 1\.2
done"
		dotest conflicts-status-4 "${testcvs} status a" \
"===================================================================
File: a                	Status: Up-to-date

   Working revision:	1\.3.*
   Repository revision:	1\.3	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

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
		mkdir first-dir/dir1 first-dir/sdir
		dotest conflicts-136 "${testcvs} -q update first-dir" \
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
		rm -r 1 2 3 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
		;;

	conflicts2)
	  # More conflicts tests; separate from conflicts to keep each
	  # test a manageable size.
	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  mkdir 1
	  cd 1

	  dotest conflicts2-142a1 "${testcvs} -q co first-dir" ''

	  cd first-dir
	  touch a abc

	  dotest conflicts2-142a2 "${testcvs} add a abc" \
"${PROG} [a-z]*: scheduling file .a. for addition
${PROG} [a-z]*: scheduling file .abc. for addition
${PROG} [a-z]*: use .cvs commit. to add these files permanently"
	  dotest conflicts2-142a3 "${testcvs} -q ci -m added" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/a,v
done
Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/abc,v
done
Checking in abc;
${TESTDIR}/cvsroot/first-dir/abc,v  <--  abc
initial revision: 1\.1
done"

	  cd ../..
	  mkdir 2
	  cd 2

	  dotest conflicts2-142a4 "${testcvs} -q co first-dir" 'U first-dir/a
U first-dir/abc'
	  cd ..

	  # Now test that if one person modifies and commits a
	  # file and a second person removes it, it is a
	  # conflict
	  cd 1/first-dir
	  echo modify a >>a
	  dotest conflicts2-142b2 "${testcvs} -q ci -m modify-a" \
"Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../../2/first-dir
	  rm a
	  dotest conflicts2-142b3 "${testcvs} rm a" \
"${PROG} [a-z]*: scheduling .a. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  dotest_fail conflicts2-142b4 "${testcvs} -q update" \
"${PROG} [a-z]*: conflict: removed a was modified by second party
C a"
	  # Resolve the conflict by deciding not to remove the file
	  # after all.
	  dotest conflicts2-142b5 "${testcvs} add a" "U a
${PROG} [a-z]*: a, version 1\.1, resurrected"
	  dotest conflicts2-142b6 "${testcvs} -q update" ''
	  cd ../..

	  # Now test that if one person removes a file and
	  # commits it, and a second person removes it, is it
	  # not a conflict.
	  cd 1/first-dir
	  rm abc
	  dotest conflicts2-142c0 "${testcvs} rm abc" \
"${PROG} [a-z]*: scheduling .abc. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  dotest conflicts2-142c1 "${testcvs} -q ci -m remove-abc" \
"Removing abc;
${TESTDIR}/cvsroot/first-dir/abc,v  <--  abc
new revision: delete; previous revision: 1\.1
done"
	  cd ../../2/first-dir
	  rm abc
	  dotest conflicts2-142c2 "${testcvs} rm abc" \
"${PROG} [a-z]*: scheduling .abc. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  dotest conflicts2-142c3 "${testcvs} update" \
"${PROG} [a-z]*: Updating \."
	  cd ../..

	  # conflicts2-142d*: test that if one party adds a file, and another
	  # party has a file of the same name, cvs notices
	  cd 1/first-dir
	  touch aa.c
	  dotest conflicts2-142d0 "${testcvs} add aa.c" \
"${PROG} [a-z]*: scheduling file .aa\.c. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest conflicts2-142d1 "${testcvs} -q ci -m added" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aa.c,v
done
Checking in aa.c;
${TESTDIR}/cvsroot/first-dir/aa.c,v  <--  aa.c
initial revision: 1\.1
done"
	  cd ../../2/first-dir
	  echo "don't you dare obliterate this text" >aa.c
	  # Doing this test separately for remote and local is a fair
	  # bit of a kludge, but the exit status differs.  I'm not sure
	  # which exit status is the more appropriate one.
	  if test "$remote" = yes; then
	    dotest conflicts2-142d2 "${testcvs} -q update" \
"${QUESTION} aa\.c
U aa\.c
${PROG} update: move away \./aa\.c; it is in the way"
	  else
	    dotest_fail conflicts2-142d2 "${testcvs} -q update" \
"${PROG} [a-z]*: move away aa\.c; it is in the way
C aa\.c"
	  fi
	  cd ../..

	  rm -r 1 2 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules)
	  # Tests of various ways to define and use modules.
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

	  echo realmodule first-dir/subdir a >CVSROOT/modules
	  echo dirmodule first-dir/subdir >>CVSROOT/modules
	  echo namedmodule -d nameddir first-dir/subdir >>CVSROOT/modules
	  echo aliasmodule -a first-dir/subdir/a >>CVSROOT/modules
	  echo aliasnested -a first-dir/subdir/ssdir >>CVSROOT/modules
	  echo topfiles -a first-dir/file1 first-dir/file2 >>CVSROOT/modules
	  echo world -a . >>CVSROOT/modules

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
	  dotest 148a0 "${testcvs} co -c" 'aliasmodule  -a first-dir/subdir/a
aliasnested  -a first-dir/subdir/ssdir
bogusalias   first-dir/subdir/a -a
dirmodule    first-dir/subdir
namedmodule  -d nameddir first-dir/subdir
realmodule   first-dir/subdir a
topfiles     -a first-dir/file1 first-dir/file2
world        -a .'
	  # I don't know why aliasmodule isn't printed (I would have thought
	  # that it gets printed without the -a; although I'm not sure that
	  # printing expansions without options is useful).
	  dotest 148a1 "${testcvs} co -s" \
'bogusalias   NONE        first-dir/subdir/a -a
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
	  rm -r dirmodule

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
	  rm -r 1

	  mkdir 2
	  cd 2
	  dotest modules-155a0 "${testcvs} co aliasnested" \
"${PROG} [a-z]*: Updating first-dir/subdir/ssdir"
	  dotest modules-155a1 "test -d first-dir" ''
	  dotest modules-155a2 "test -d first-dir/subdir" ''
	  dotest modules-155a3 "test -d first-dir/subdir/ssdir" ''
	  # Test that nothing extraneous got created.
	  dotest modules-155a4 "ls" "first-dir" \
"CVS
first-dir"
	  cd ..
	  rm -r 2

	  # Test checking out everything.
	  mkdir 1
	  cd 1
	  dotest modules-155b "${testcvs} -q co world" \
"U CVSROOT/${DOTSTAR}
U first-dir/subdir/a
U first-dir/subdir/b"
	  cd ..
	  rm -r 1

	  # Test checking out a module which lists at least two
	  # specific files twice.  At one time, this failed over
	  # remote CVS.
	  mkdir 1
	  cd 1
	  dotest modules-155c1 "${testcvs} -q co first-dir" \
"U first-dir/subdir/a
U first-dir/subdir/b"

	  cd first-dir
	  echo 'first revision' > file1
	  echo 'first revision' > file2
	  dotest modules-155c2 "${testcvs} add file1 file2" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: scheduling file `file2'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add these files permanently'
	  dotest modules-155c3 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  cd ..
	  rm -r first-dir
	  dotest modules-155c4 "${testcvs} -q co topfiles" \
"U first-dir/file1
U first-dir/file2"
	  dotest modules-155c5 "${testcvs} -q co topfiles" ""
	  cd ..
	  rm -r 1

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules2)
	  # More tests of modules, in particular the & feature.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir ${CVSROOT_DIRNAME}/second-dir

	  mkdir 1
	  cd 1

	  dotest modules2-1 "${testcvs} -q co CVSROOT/modules" \
'U CVSROOT/modules'
	  cd CVSROOT
	  echo 'ampermodule &first-dir &second-dir' > modules
	  # Depending on whether the user also ran the modules test
	  # we will be checking in revision 1.2 or 1.3.
	  dotest modules2-2 "${testcvs} -q ci -m add-modules" \
"Checking in modules;
${TESTDIR}/cvsroot/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"

	  cd ..

	  dotest modules2-3 "${testcvs} -q co ampermodule" ''
	  dotest modules2-4 "test -d ampermodule/first-dir" ''
	  dotest modules2-5 "test -d ampermodule/second-dir" ''

	  # Test ability of cvs release to handle multiple arguments
	  cd ampermodule
	  if ${testcvs} release -d first-dir second-dir <<EOF >>${LOGFILE}
yes
yes
EOF
	  then
	    pass modules2-6
	  else
	    fail modules2-6
	  fi
	  dotest_fail modules2-7 "test -d first-dir" ''
	  dotest_fail modules2-8 "test -d second-dir" ''

	  cd ..

	  # Test that CVS gives an error if one combines -a with
	  # other options.
	  cd CVSROOT
	  echo 'aliasopt -a -d onedir first-dir' >modules
	  dotest modules2-a0 "${testcvs} -q ci -m add-modules" \
"Checking in modules;
${TESTDIR}/cvsroot/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..
	  dotest_fail modules2-a1 "${testcvs} -q co aliasopt" \
"${PROG} [a-z]*: -a cannot be specified in the modules file along with other options" \
"${PROG} [a-z]*: -a cannot be specified in the modules file along with other options
${PROG} \[[a-z]* aborted\]: cannot expand modules"

	  # Clean up.
	  rm -r CVSROOT
	  cd ..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -rf ${CVSROOT_DIRNAME}/second-dir
	  ;;

	modules3)
	  # More tests of modules, in particular what happens if several
	  # modules point to the same file.

	  # First just set up a directory first-dir and a file file1 in it.
	  mkdir 1; cd 1

	  dotest modules3-0 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest modules3-1 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"

	  cd first-dir
	  echo file1 >file1
	  dotest modules3-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest modules3-3 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..

	  dotest modules3-4 "${testcvs} -q co CVSROOT/modules" \
'U CVSROOT/modules'
	  cd CVSROOT
	  cat >modules <<EOF
mod1 -a first-dir/file1
bigmod -a mod1 first-dir/file1
namednest -d src/sub/dir first-dir
nestdeeper -d src/sub1/sub2/sub3/dir first-dir
nestshallow -d src/dir second-dir/suba/subb
path/in/modules &mod1
EOF
	  dotest modules3-5 "${testcvs} -q ci -m add-modules" \
"Checking in modules;
${TESTDIR}/cvsroot/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..

	  dotest modules3-6 "${testcvs} -q co bigmod" ''
	  rm -r first-dir
	  dotest modules3-7 "${testcvs} -q co bigmod" 'U first-dir/file1'
	  cd ..
	  rm -r 1

	  mkdir 1; cd 1
	  mkdir suba
	  mkdir suba/subb
	  # This fails to work remote (it doesn't notice the directories,
	  # I suppose because they contain no files).  Bummer, especially
	  # considering this is a documented technique and everything.
	  dotest modules3-7a \
"${testcvs} import -m add-dirs second-dir tag1 tag2" \
"${PROG} [a-z]*: Importing ${TESTDIR}/cvsroot/second-dir/suba
${PROG} [a-z]*: Importing ${TESTDIR}/cvsroot/second-dir/suba/subb

No conflicts created by this import" "
No conflicts created by this import"
	  cd ..; rm -r 1
	  mkdir 1; cd 1
	  dotest modules3-7b "${testcvs} co second-dir" \
"${PROG} [a-z]*: Updating second-dir
${PROG} [a-z]*: Updating second-dir/suba
${PROG} [a-z]*: Updating second-dir/suba/subb" \
"${PROG} server: Updating second-dir"

	  if test "x$remote" = xyes; then
	    cd second-dir
	    mkdir suba
	    dotest modules3-7-workaround1 "${testcvs} add suba" \
"Directory ${TESTDIR}/cvsroot/second-dir/suba added to the repository"
	    cd suba
	    mkdir subb
	    dotest modules3-7-workaround2 "${testcvs} add subb" \
"Directory ${TESTDIR}/cvsroot/second-dir/suba/subb added to the repository"
	    cd ../..
	  fi

	  cd second-dir/suba/subb
	  touch fileb
	  dotest modules3-7c "${testcvs} add fileb" \
"${PROG} [a-z]*: scheduling file .fileb. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest modules3-7d "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/second-dir/suba/subb/fileb,v
done
Checking in fileb;
${TESTDIR}/cvsroot/second-dir/suba/subb/fileb,v  <--  fileb
initial revision: 1\.1
done"
	  cd ../../..
	  cd ..; rm -r 1

	  mkdir 1; cd 1
	  dotest modules3-7e "${testcvs} -q co nestshallow" \
"U src/dir/fileb"

	  # Using ${TESTDIR}/cvsroot/second-dir/suba instead of
	  # ${TESTDIR}/cvsroot/second-dir seems wrong, it seems like the
	  # 30 Dec 1996 change to build_dirs_and_chdir simply failed
	  # to consider what to put in CVS/Repository.
	  # Remote does "${TESTDIR}/cvsroot/\." which seems equally wrong,
	  # if in a different way, but variety is the spice of life,
	  # eh?
	  dotest modules3-7f "cat CVS/Repository" \
"${TESTDIR}/cvsroot/second-dir/suba" "${TESTDIR}/cvsroot/\."

	  dotest modules3-7g "cat src/CVS/Repository" \
"${TESTDIR}/cvsroot/second-dir/suba"
	  dotest modules3-7h "cat src/dir/CVS/Repository" \
"${TESTDIR}/cvsroot/second-dir/suba/subb"
	  cd ..; rm -r 1

	  mkdir 1
	  cd 1
	  dotest modules3-8 "${testcvs} -q co namednest" \
'U src/sub/dir/file1'
	  dotest modules3-9 "test -f src/sub/dir/file1" ''
	  cd ..
	  rm -r 1

	  # Try the same thing, but with the directories nested even
	  # deeper (deeply enough so they are nested more deeply than
	  # the number of directories from / to ${TESTDIR}).
	  mkdir 1
	  cd 1
	  dotest modules3-10 "${testcvs} -q co nestdeeper" \
'U src/sub1/sub2/sub3/dir/file1'
	  dotest modules3-11 "test -f src/sub1/sub2/sub3/dir/file1" ''

	  # While we are doing things like twisted uses of '/' (e.g.
	  # modules3-12), try this one.
	  if test "x$remote" = xyes; then
	    dotest_fail modules3-11b \
"${testcvs} -q update ${TESTDIR}/1/src/sub1/sub2/sub3/dir/file1" \
"${PROG} .server aborted.: absolute pathname .${TESTDIR}/1/src/sub1/sub2/sub3/dir/file1. illegal for server"
	  fi # end of remote-only tests

	  cd ..
	  rm -r 1

	  # This one is almost too twisted for words.  The pathname output
	  # in the message from "co" doesn't include the "path/in/modules",
	  # but those directories do get created (with no CVSADM except
	  # in "modules" which has a CVSNULLREPOS).
	  # I'm not sure anyone is relying on this nonsense or whether we
	  # need to keep doing it, but it is what CVS currently does...
	  # Skip it for remote; the remote code has the good sense to
	  # not deal with it (on the minus side it gives
	  # "internal error: repository string too short." instead of a
	  # real error).
	  # I kind of suspect that it would be OK to just make it a fatal
	  # error to have '/' in a module name.
	  if test "x$remote" = xno; then
	  mkdir 1; cd 1
	  dotest modules3-12 "${testcvs} -q co path/in/modules" \
"U first-dir/file1"
	  dotest modules3-13 "test -f path/in/modules/first-dir/file1" ''
	  cd ..; rm -r 1

	  # Now here is where it gets seriously bogus.
	  mkdir 1; cd 1
	  dotest modules3-14 \
"${testcvs} -q rtag tag1 path/in/modules" ''
	  # CVS creates this even though rtag should *never* affect
	  # the directory current when it is called!
	  dotest modules3-15 "test -d path/in/modules" ''
	  # Just for trivia's sake, rdiff is not similarly vulnerable
	  # because it passes 0 for run_module_prog to do_module.
	  cd ..; rm -r 1
	  fi # end of tests skipped for remote

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -rf ${CVSROOT_DIRNAME}/second-dir
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
	    cd ..; rm -r a-dir
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
	    cd ..
	    rm -r a-dir
	    rm -rf ${CVSROOT_DIRNAME}/a-dir
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
	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/1dir
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

	  # FIXME: This probably should be an error message instead
	  # of silently succeeding and printing nothing.
	  dotest devcom-a-nonexist "${testcvs} watchers nonexist" ''

	  dotest devcom-a1 "${testcvs} watch add" ''
	  dotest devcom-a2 "${testcvs} watchers" \
'abb	[a-z0-9]*	edit	unedit	commit
abc	[a-z0-9]*	edit	unedit	commit'
	  dotest devcom-a3 "${testcvs} watch remove -a unedit abb" ''
	  dotest devcom-a4 "${testcvs} watchers abb" \
'abb	[a-z0-9]*	edit	commit'

	  # Check tagging and checking out while we have a CVS
	  # directory in the repository.
	  dotest devcom-t0 "${testcvs} -q tag tag" \
'T abb
T abc'
	  cd ../..
	  mkdir 3
	  cd 3

	  # Test commented out because the bug it tests for is not fixed
	  # The error is:
	  # cvs watchers: cannot open CVS/Entries for reading: No such file or directory
	  # cvs: ../../work/ccvs/src/fileattr.c:75: fileattr_read: Assertion `fileattr_stored_repos != ((void *)0)' failed.
:	  dotest devcom-t-nonexist "${testcvs} watchers nonexist" fixme

	  dotest devcom-t1 "${testcvs} -q co -rtag first-dir/abb" \
'U first-dir/abb'
	  cd ..
	  # Since first-dir/abb is readonly, use -f.
	  rm -rf 3

	  # Now remove all the file attributes
	  cd 2/first-dir
	  dotest devcom-b0 "${testcvs} watch off" ''
	  dotest devcom-b1 "${testcvs} watch remove" ''
	  # Test that CVS 1.6 and earlier can handle the repository.
	  dotest_fail devcom-b2 "test -d ${CVSROOT_DIRNAME}/first-dir/CVS"

	  # Now test watching just some, not all, files.
	  dotest devcom-some0 "${testcvs} watch on abc" ''
	  cd ../..
	  mkdir 3
	  cd 3
	  dotest devcom-some1 "${testcvs} -q co first-dir" 'U first-dir/abb
U first-dir/abc'
	  dotest devcom-some2 "test -w first-dir/abb" ''
	  dotest_fail devcom-some3 "test -w first-dir/abc" ''
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  # Use -f because of the readonly files.
	  rm -rf 1 2 3
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	devcom2)
	  # More watch tests, most notably setting watches on
	  # files in various different states.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest devcom2-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # This should probably be an error; setting a watch on a totally
	  # unknown file is more likely to be a typo than intentional.
	  # But that isn't the currently implemented behavior.
	  dotest devcom2-2 "${testcvs} watch on w1" ''

	  touch w1 w2 w3 nw1
	  dotest devcom2-3 "${testcvs} add w1 w2 w3 nw1" "${DOTSTAR}"
	  # Letting the user set the watch here probably can be considered
	  # a feature--although it leads to a few potentially strange
	  # consequences like one user can set the watch and another actually
	  # adds the file.
	  dotest devcom2-4 "${testcvs} watch on w2" ''
	  dotest devcom2-5 "${testcvs} -q ci -m add-them" "${DOTSTAR}"

	  # Note that this test differs in a subtle way from devcom-some0;
	  # in devcom-some0 the watch is creating a new fileattr file, and
	  # here we are modifying an existing one.
	  dotest devcom2-6 "${testcvs} watch on w3" ''

	  # Now test that all the watches got set on the correct files
	  # FIXME: CVS should have a way to report whether watches are
	  # set, I think.  The "check it out and see if it read-only" is
	  # sort of OK, but is complicated by CVSREAD and doesn't help
	  # if the file is added and not yet committed or some such.
	  # Probably "cvs status" should report "watch: on" if watch is on
	  # (and nothing if watch is off, so existing behavior is preserved).
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest devcom2-7 "${testcvs} -q co first-dir" 'U first-dir/nw1
U first-dir/w1
U first-dir/w2
U first-dir/w3'
	  dotest devcom2-8 "test -w first-dir/nw1" ''
	  dotest_fail devcom2-9 "test -w first-dir/w1" ''
	  dotest_fail devcom2-10 "test -w first-dir/w2" ''
	  dotest_fail devcom2-11 "test -w first-dir/w3" ''
	  cd ..

	  # Use -f because of the readonly files.
	  rm -rf 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	devcom3)
	  # More watch tests, most notably handling of features designed
	  # for future expansion.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest devcom3-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  touch w1 w2
	  dotest devcom3-2 "${testcvs} add w1 w2" "${DOTSTAR}"
	  dotest devcom3-3 "${testcvs} watch on w1 w2" ''
	  dotest devcom3-4 "${testcvs} -q ci -m add-them" "${DOTSTAR}"

	  # OK, since we are about to delve into CVS's internals, make
	  # sure that we seem to be correct about how they work.
	  dotest devcom3-5 "cat ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr" \
"Fw1	_watched=
Fw2	_watched="
	  # Now write a few more lines, just as if we were a newer version
	  # of CVS implementing some new feature.
	  cat <<EOF >>${CVSROOT_DIRNAME}/first-dir/CVS/fileattr
Enew	line	here
G@#$^!@#=&
EOF
	  # Now get CVS to write to the fileattr file....
	  dotest devcom3-6 "${testcvs} watch off w1" ''
	  # ...and make sure that it hasn't clobbered our new lines.
	  # Note that writing these lines in another order would be OK
	  # too.
	  dotest devcom3-7 "cat ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr" \
"Fw2	_watched=
G@#..!@#=&
Enew	line	here"

	  # See what CVS does when a file name is duplicated.  The
	  # behavior of all versions of CVS since file attributes were
	  # implemented is that it nukes the duplications.  This seems
	  # reasonable enough, although it means it isn't clear how
	  # useful duplicates would be for purposes of future
	  # expansion.  But in the interests of keeping behaviors
	  # predictable, might as well test for it, I guess.
	  echo 'Fw2	duplicate=' >>${CVSROOT_DIRNAME}/first-dir/CVS/fileattr
	  dotest devcom3-8 "${testcvs} watch on w1" ''
	  dotest devcom3-9 "cat ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr" \
"Fw2	_watched=
Fw1	_watched=
Enew	line	here
G@#..!@#=&"

	  cd ../..

	  # Use -f because of the readonly files.
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	ignore)
	  dotest 187a1 "${testcvs} -q co CVSROOT" "U CVSROOT/${DOTSTAR}"
	  cd CVSROOT
	  echo rootig.c >cvsignore
	  dotest 187a2 "${testcvs} add cvsignore" "${PROG}"' [a-z]*: scheduling file `cvsignore'"'"' for addition
'"${PROG}"' [a-z]*: use '"'"'cvs commit'"'"' to add this file permanently'

	  # As of Jan 96, local CVS prints "Examining ." and remote doesn't.
	  # Accept either.
	  dotest 187a3 " ${testcvs} ci -m added" \
"${DOTSTAR}CS file: ${TESTDIR}/cvsroot/CVSROOT/cvsignore,v
done
Checking in cvsignore;
${TESTDIR}/cvsroot/CVSROOT/cvsignore,v  <--  cvsignore
initial revision: 1\.1
done
${PROG} [a-z]*: Rebuilding administrative file database"

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
	  rm -r dir-to-import

	  mkdir 1
	  cd 1
	  dotest 189a "${testcvs} -q co second-dir" \
'U second-dir/bar.c
U second-dir/defig.o
U second-dir/envig.c
U second-dir/foobar.c
U second-dir/optig.c
U second-dir/rootig.c'
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

	  # Now test that commands other than update also print "? notig.c"
	  # where appropriate.  Only test this for remote, because local
	  # CVS only prints it on update.
	  rm optig.c
	  if test "x$remote" = xyes; then
	    dotest 189e "${testcvs} -q diff" "${QUESTION} notig.c"

	    # Force the server to be contacted.  Ugh.  Having CVS
	    # contact the server for the sole purpose of checking
	    # the CVSROOT/cvsignore file does not seem like such a
	    # good idea, so I imagine this will continue to be
	    # necessary.  Oh well, at least we test CVS's ablity to
	    # handle a file with a modified timestamp but unmodified
	    # contents.
	    touch bar.c

	    dotest 189f "${testcvs} -q ci -m commit-it" "${QUESTION} notig.c"
	  fi

	  # now test .cvsignore files
	  cd ..
	  echo notig.c >first-dir/.cvsignore
	  echo foobar.c >second-dir/.cvsignore
	  touch first-dir/notig.c second-dir/notig.c second-dir/foobar.c
	  dotest 190 "${testcvs} -qn update" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/notig.c
${QUESTION} second-dir/.cvsignore"
	  dotest 191 "${testcvs} -qn update -I! -I CVS" \
"${QUESTION} first-dir/rootig.c
${QUESTION} first-dir/defig.o
${QUESTION} first-dir/envig.c
${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c" \
"${QUESTION} first-dir/rootig.c
${QUESTION} first-dir/defig.o
${QUESTION} first-dir/envig.c
${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/notig.c
${QUESTION} second-dir/.cvsignore"

	  if echo yes | ${testcvs} release -d first-dir \
	    >${TESTDIR}/ignore.tmp; then
	    pass ignore-192
	  else
	    fail ignore-192
	  fi
	  dotest ignore-193 "cat ${TESTDIR}/ignore.tmp" \
"${QUESTION} \.cvsignore
You have \[0\] altered files in this repository.
Are you sure you want to release (and delete) directory .first-dir': "

	  echo add a line >>second-dir/foobar.c
	  rm second-dir/notig.c second-dir/.cvsignore
	  if echo yes | ${testcvs} release -d second-dir \
	    >${TESTDIR}/ignore.tmp; then
	    pass ignore-194
	  else
	    fail ignore-194
	  fi
	  dotest ignore-195 "cat ${TESTDIR}/ignore.tmp" \
"M foobar.c
You have \[1\] altered files in this repository.
Are you sure you want to release (and delete) directory .second-dir': "
	  cd ..
	  rm -r 1
	  rm ${TESTDIR}/ignore.tmp
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
"RCS file: ${TESTDIR}/cvsroot/first-dir/binfile,v
done
Checking in binfile;
${TESTDIR}/cvsroot/first-dir/binfile,v  <--  binfile
initial revision: 1\.1
done"
	  cd ../..
	  mkdir 2; cd 2
	  dotest binfiles-4 "${testcvs} -q co first-dir" 'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-5 "cmp ../../1/binfile.dat binfile" ''
	  # Testing that sticky options is -kb is the closest thing we have
	  # to testing that binary files work right on non-unix machines
	  # (until there is automated testing for such machines, of course).
	  dotest binfiles-5.5 "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"

	  # Test whether the default options from the RCS file are
	  # also used when operating on files instead of whole
	  # directories
          cd ../..
	  mkdir 3; cd 3
	  dotest binfiles-5.5b0 "${testcvs} -q co first-dir/binfile" \
'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-5.5b1 "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  cd ../..
	  rm -r 3
	  cd 2/first-dir

	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-6 "${testcvs} -q ci -m modify-it" \
"Checking in binfile;
${TESTDIR}/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../../1/first-dir
	  dotest binfiles-7 "${testcvs} -q update" '[UP] binfile'
	  dotest binfiles-8 "cmp ../binfile2.dat binfile" ''

	  # Now test handling of conflicts with binary files.
	  cp ../binfile.dat binfile
	  dotest binfiles-con0 "${testcvs} -q ci -m modify-it" \
"Checking in binfile;
${TESTDIR}/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.3; previous revision: 1\.2
done"
	  cd ../../2/first-dir
	  echo 'edits in dir 2' >binfile
	  dotest binfiles-con1 "${testcvs} -q update" \
"U binfile
${PROG} [a-z]*: binary file needs merge
${PROG} [a-z]*: revision 1\.3 from repository is now in binfile
${PROG} [a-z]*: file from working directory is now in \.#binfile\.1\.2
C binfile"
	  dotest binfiles-con2 "cmp binfile ../../1/binfile.dat" ''
	  dotest binfiles-con3 "cat .#binfile.1.2" 'edits in dir 2'

	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-con4 "${testcvs} -q ci -m resolve-it" \
"Checking in binfile;
${TESTDIR}/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.4; previous revision: 1\.3
done"
	  cd ../../1/first-dir
	  dotest binfiles-con5 "${testcvs} -q update" '[UP] binfile'

	  # The bugs which these test for are apparently not fixed for remote.
	  if test "$remote" = no; then
	    dotest binfiles-9 "${testcvs} -q update -A" ''
	    dotest binfiles-10 "${testcvs} -q update -kk" '[UP] binfile'
	    dotest binfiles-11 "${testcvs} -q update" ''
	    dotest binfiles-12 "${testcvs} -q update -A" '[UP] binfile'
	    dotest binfiles-13 "${testcvs} -q update -A" ''
	  fi

	  cd ../..
	  rm -r 1

	  mkdir 3
	  cd 3
	  dotest binfiles-13a0 "${testcvs} -q co -r HEAD first-dir" \
'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-13a1 "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.4.*
   Repository revision:	1\.4	${TESTDIR}/cvsroot/first-dir/binfile,v
   Sticky Tag:		HEAD (revision: 1\.4)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  cd ../..
	  rm -r 3

	  cd 2/first-dir
	  echo 'this file is $''RCSfile$' >binfile
	  dotest binfiles-14a "${testcvs} -q ci -m modify-it" \
"Checking in binfile;
${TESTDIR}/cvsroot/first-dir/binfile,v  <--  binfile
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest binfiles-14b "cat binfile" 'this file is $''RCSfile$'
	  # See binfiles-5.5 for discussion of -kb.
	  dotest binfiles-14c "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	${TESTDIR}/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  dotest binfiles-14d "${testcvs} admin -kv binfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/binfile,v
done"
	  # cvs admin doesn't change the checked-out file or its sticky
	  # kopts.  There probably should be a way which does (but
	  # what if the file is modified?  And do we try to version
	  # control the kopt setting?)
	  dotest binfiles-14e "cat binfile" 'this file is $''RCSfile$'
	  dotest binfiles-14f "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	${TESTDIR}/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  dotest binfiles-14g "${testcvs} -q update -A" '[UP] binfile'
	  dotest binfiles-14h "cat binfile" 'this file is binfile,v'
	  dotest binfiles-14i "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	${TESTDIR}/cvsroot/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kv"

	  # Do sticky options work when used with 'cvs update'?
	  echo "Not a binary file." > nibfile
	  dotest binfiles-sticky1 "${testcvs} -q add nibfile" \
"${PROG} [a-z]*: use "\''cvs commit'\'' to add this file permanently'
	  dotest binfiles-sticky2 "${testcvs} -q ci -m add-it nibfile" \
	    "RCS file: ${TESTDIR}/cvsroot/first-dir/nibfile,v
done
Checking in nibfile;
${TESTDIR}/cvsroot/first-dir/nibfile,v  <--  nibfile
initial revision: 1\.1
done"
	  dotest binfiles-sticky3 "${testcvs} -q update -kb nibfile" \
	    '[UP] nibfile'
	  dotest binfiles-sticky4 "${testcvs} -q status nibfile" \
"===================================================================
File: nibfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/nibfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  # Eventually we should test that -A removes the -kb here...

	  cd ../..
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 2
	  ;;

	binfiles2)
	  # Test cvs's ability to handle binary files, particularly branching
	  # and joining.  The key thing we are worrying about is that CVS
	  # doesn't print "cannot merge binary files" or some such, in 
	  # situations where no merging is required.
	  # See also "join" which does this with non-binary files.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest binfiles2-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  # FIXCVS: unless a branch has at least one file on it,
	  # tag_check_valid won't know it exists.  So creating a
	  # file here is a workaround.
	  touch dummy
	  dotest binfiles2-1a "${testcvs} add dummy" \
"${PROG} [a-z]*: scheduling file .dummy. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest binfiles2-1b "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/dummy,v
done
Checking in dummy;
${TESTDIR}/cvsroot/first-dir/dummy,v  <--  dummy
initial revision: 1\.1
done"
	  dotest binfiles2-2 "${testcvs} -q tag -b br" 'T dummy'
	  dotest binfiles2-3 "${testcvs} -q update -r br" ''
	  awk 'BEGIN { printf "%c%c%c%c%c%c", 2, 10, 137, 0, 13, 10 }' \
	    </dev/null >../binfile
	  cp ../binfile binfile.dat
	  dotest binfiles2-4 "${testcvs} add -kb binfile.dat" \
"${PROG} [a-z]*: scheduling file .binfile\.dat. for addition on branch .br.
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest binfiles2-5 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/binfile\.dat,v
done
Checking in binfile\.dat;
${TESTDIR}/cvsroot/first-dir/Attic/binfile\.dat,v  <--  binfile\.dat
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest binfiles2-6 "${testcvs} -q update -A" \
"${PROG} [a-z]*: warning: binfile\.dat is not (any longer) pertinent"
	  dotest_fail binfiles2-7 "test -f binfile.dat" ''
	  dotest binfiles2-8 "${testcvs} -q update -j br" "U binfile.dat"
	  dotest binfiles2-9 "cmp ../binfile binfile.dat"
	  cd ..
	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 1
	  ;;

	binwrap)
	  # Test the ability to specify binary-ness based on file name.
	  # We could also be testing the ability to use the other
	  # ways to specify a wrapper (CVSROOT/cvswrappers, etc.).

	  mkdir dir-to-import
	  cd dir-to-import
	  touch foo.c foo.exe

	  # While we're here, test for rejection of duplicate tag names.
	  dotest_fail binwrap-0 \
	    "${testcvs} import -m msg -I ! first-dir dup dup" \
"${PROG} \[[a-z]* aborted\]: tag .dup. was specified more than once"

	  if ${testcvs} import -m message -I ! -W "*.exe -k 'b'" \
	      first-dir tag1 tag2 >>${LOGFILE}; then
	    pass binwrap-1
	  else
	    fail binwrap-1
	  fi
	  cd ..
	  rm -r dir-to-import
	  dotest binwrap-2 "${testcvs} -q co first-dir" 'U first-dir/foo.c
U first-dir/foo.exe'
	  dotest binwrap-3 "${testcvs} -q status first-dir" \
"===================================================================
File: foo\.c            	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	${TESTDIR}/cvsroot/first-dir/foo\.c,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: foo\.exe          	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	${TESTDIR}/cvsroot/first-dir/foo\.exe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	mwrap)
	  # Tests relating to the -m wrappers options.  -k tests are in
	  # binwrap and -t/-f tests haven't been written yet.
	  dotest mwrap-c1 "${testcvs} -q co CVSROOT" "[UP] CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  echo "* -m 'COPY'" >>cvswrappers
	  dotest mwrap-c2 "${testcvs} -q ci -m wrapper-mod" \
"Checking in cvswrappers;
${TESTDIR}/cvsroot/CVSROOT/cvswrappers,v  <--  cvswrappers
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..
	  mkdir m1; cd m1
	  dotest mwrap-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest mwrap-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  dotest mwrap-3 "${testcvs} add aa" \
"${PROG} [a-z]*: scheduling file .aa. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest mwrap-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aa,v
done
Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  cd ../..
	  mkdir m2; cd m2
	  dotest mwrap-5 "${testcvs} -q co first-dir" "U first-dir/aa"
	  cd first-dir
	  echo "changed in m2" >aa
	  dotest mwrap-6 "${testcvs} -q ci -m m2-mod" \
"Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../..
	  cd m1/first-dir
	  echo "changed in m1" >aa
	  dotest_fail mwrap-7 "${testcvs} -nq update" "C aa"
	  dotest_fail mwrap-8 "${testcvs} -q update" \
"${PROG} [a-z]*: A -m .COPY. wrapper is specified
${PROG} [a-z]*: but file aa needs merge
${PROG} \[[a-z]* aborted\]: You probably want to avoid -m .COPY. wrappers"
	  # Under the old, dangerous behavior, this would have been
	  # "changed in m2" -- that is, the changes in the working directory
	  # would have been clobbered (!).
	  dotest mwrap-9 "cat aa" "changed in m1"
	  cd ../..
	  cd CVSROOT
	  echo '# comment out' >cvswrappers
	  dotest mwrap-ce "${testcvs} -q ci -m wrapper-mod" \
"Checking in cvswrappers;
${TESTDIR}/cvsroot/CVSROOT/cvswrappers,v  <--  cvswrappers
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..
	  rm -r CVSROOT
	  rm -r m1 m2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	info)
	  # Test CVS's ability to handle *info files.
	  dotest info-1 "${testcvs} -q co CVSROOT" "[UP] CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  echo "ALL sh -c \"echo x\${=MYENV}\${=OTHER}y\${=ZEE}=\$USER=\$CVSROOT= >>$TESTDIR/testlog; cat >/dev/null\"" > loginfo
          # The following cases test the format string substitution
          echo "ALL echo %{sVv} >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %{v} >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %s >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %{V}AX >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %sux >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo

	  # Might be nice to move this to crerepos tests; it should
	  # work to create a loginfo file if you didn't create one
	  # with "cvs init".
	  : dotest info-2 "${testcvs} add loginfo" \
"${PROG}"' [a-z]*: scheduling file `loginfo'"'"' for addition
'"${PROG}"' [a-z]*: use '"'"'cvs commit'"'"' to add this file permanently'

	  dotest info-3 "${testcvs} -q ci -m new-loginfo" \
"Checking in loginfo;
${TESTDIR}/cvsroot/CVSROOT/loginfo,v  <--  loginfo
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest info-5 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  touch file1
	  dotest info-6 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  echo "cvs -s OTHER=not-this -s MYENV=env-" >>$HOME/.cvsrc
	  dotest info-6a "${testcvs} -q -s OTHER=value ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
${PROG} [a-z]*: loginfo:1: no such user variable \${=ZEE}"
	  echo line1 >>file1
	  dotest info-7 "${testcvs} -q -s OTHER=value -s ZEE=z ci -m mod-it" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ..
	  dotest info-9 "cat $TESTDIR/testlog" "xenv-valueyz=[a-z0-9@][a-z0-9@]*=${TESTDIR}/cvsroot="
          dotest info-10 "cat $TESTDIR/testlog2" 'first-dir file1,NONE,1.1
first-dir 1.1
first-dir file1
first-dir NONEAX
first-dir file1ux
first-dir file1,1.1,1.2
first-dir 1.2
first-dir file1
first-dir 1.1AX
first-dir file1ux'

	  cd CVSROOT
	  echo '# do nothing' >loginfo
	  dotest info-11 "${testcvs} -q -s ZEE=garbage ci -m nuke-loginfo" \
"Checking in loginfo;
${TESTDIR}/cvsroot/CVSROOT/loginfo,v  <--  loginfo
new revision: 1\.[0-9]; previous revision: 1\.[0-9]
done
${PROG} [a-z]*: Rebuilding administrative file database"

	  # Now test verifymsg
	  cat >${TESTDIR}/vscript <<EOF
#!/bin/sh
if head -1 < \$1 | grep '^BugId:[ ]*[0-9][0-9]*$' > /dev/null; then
    exit 0
else
    echo "No BugId found."
    exit 1
fi
EOF
	  chmod +x ${TESTDIR}/vscript
	  echo "^first-dir ${TESTDIR}/vscript" >>verifymsg
	  dotest info-v1 "${testcvs} -q ci -m add-verification" \
"Checking in verifymsg;
${TESTDIR}/cvsroot/CVSROOT/verifymsg,v  <--  verifymsg
new revision: 1\.2; previous revision: 1\.1
done
${PROG} [a-z]*: Rebuilding administrative file database"

	  cd ../first-dir
	  echo line2 >>file1
	  dotest_fail info-v2 "${testcvs} -q ci -m bogus" \
"No BugId found\.
${PROG} \[[a-z]* aborted\]: Message verification failed"

	  cat >${TESTDIR}/comment.tmp <<EOF
BugId: 42
and many more lines after it
EOF
	  dotest info-v3 "${testcvs} -q ci -F ${TESTDIR}/comment.tmp" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  cd ..
	  mkdir another-dir
	  cd another-dir
	  touch file2
	  dotest_fail info-v4 \
	    "${testcvs} import -m bogus first-dir/another x y" \
"No BugId found\.
${PROG} \[[a-z]* aborted\]: Message verification failed"
	  rm file2
	  cd ..
	  rmdir another-dir

	  cd CVSROOT
	  echo '# do nothing' >verifymsg
	  dotest info-cleanup-verifymsg "${testcvs} -q ci -m nuke-verifymsg" \
"Checking in verifymsg;
${TESTDIR}/cvsroot/CVSROOT/verifymsg,v  <--  verifymsg
new revision: 1\.[0-9]; previous revision: 1\.[0-9]
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..

	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	    pass info-cleanup
	  else
	    fail info-cleanup
	  fi
	  if echo "yes" | ${testcvs} release -d first-dir >>${LOGFILE} ; then
	    pass info-cleanup-2
	  else
	    fail info-cleanup-2
	  fi
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	serverpatch)
	  # Test remote CVS handling of unpatchable files.  This isn't
	  # much of a test for local CVS.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest serverpatch-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add a file with an RCS keyword.
	  echo '$''Name$' > file1
	  echo '1' >> file1
	  dotest serverpatch-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  dotest serverpatch-3 "${testcvs} -q commit -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  # Tag the file.
	  dotest serverpatch-4 "${testcvs} -q tag tag file1" 'T file1'

	  # Check out a tagged copy of the file.
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest serverpatch-5 "${testcvs} -q co -r tag first-dir" \
'U first-dir/file1'

	  # Remove the tag.  This will leave the tag string in the
	  # expansion of the Name keyword.
	  dotest serverpatch-6 "${testcvs} -q update -A" ''

	  # Modify and check in the first copy.
	  cd ../1/first-dir
	  echo '2' >> file1
	  dotest serverpatch-7 "${testcvs} -q ci -mx file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"

	  # Now update the second copy.  When using remote CVS, the
	  # patch will fail, forcing the file to be refetched.
	  cd ../../2/first-dir
	  dotest serverpatch-8 "${testcvs} -q update" \
'U file1' \
'P file1
'"${PROG}"' [a-z]*: checksum failure after patch to ./file1; will refetch
'"${PROG}"' [a-z]*: refetching unpatchable files
U file1'

	  cd ../..
	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	log)
	  # Test selecting revisions with cvs log.
	  # See also log2 tests for more tests.
	  # See also rcs tests, for -d option to log.
	  # See also branches-14.3 for logging with a branch off of a branch.
	  # See also multibranch-14 for logging with several branches off the
	  #   same branchpoint.

	  # Check in a file with a few revisions and branches.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest log-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 'first revision' > file1
	  dotest log-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'

	  dotest log-3 "${testcvs} -q commit -m 1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  echo 'second revision' > file1
	  dotest log-4 "${testcvs} -q ci -m2 file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"

	  dotest log-5 "${testcvs} -q tag -b branch file1" 'T file1'

	  echo 'third revision' > file1
	  dotest log-6 "${testcvs} -q ci -m3 file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"

	  dotest log-7 "${testcvs} -q update -r branch" '[UP] file1'

	  echo 'first branch revision' > file1
	  dotest log-8 "${testcvs} -q ci -m1b file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"

	  dotest log-9 "${testcvs} -q tag tag file1" 'T file1'

	  echo 'second branch revision' > file1
	  dotest log-10 "${testcvs} -q ci -m2b file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done"

	  # Set up a bunch of shell variables to make the later tests
	  # easier to describe.=
	  log_header="
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:"
	  log_tags='symbolic names:
	tag: 1\.2\.2\.1
	branch: 1\.2\.0\.2'
	  log_header2='keyword substitution: kv'
	  log_dash='----------------------------
revision'
	  log_date="date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;"
	  log_lines="  lines: ${PLUS}1 -1"
	  log_rev1="${log_dash} 1\.1
${log_date}
1"
	  log_rev2="${log_dash} 1\.2
${log_date}${log_lines}
branches:  1\.2\.2;
2"
	  log_rev3="${log_dash} 1\.3
${log_date}${log_lines}
3"
	  log_rev1b="${log_dash} 1\.2\.2\.1
${log_date}${log_lines}
1b"
	  log_rev2b="${log_dash} 1\.2\.2\.2
${log_date}${log_lines}
2b"
	  log_trailer='============================================================================='

	  # Now, finally, test the log output.

	  dotest log-11 "${testcvs} log file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-12 "${testcvs} log -N file1" \
"${log_header}
${log_header2}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-13 "${testcvs} log -b file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 3
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-14 "${testcvs} log -r file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-15 "${testcvs} log -r1.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-16 "${testcvs} log -r1.2.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  # This test would fail with the old invocation of rlog, but it
	  # works with the builtin log support.
	  dotest log-17 "${testcvs} log -rbranch file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-18 "${testcvs} log -r1.2.2. file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  # This test would fail with the old invocation of rlog, but it
	  # works with the builtin log support.
	  dotest log-19 "${testcvs} log -rbranch. file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  dotest log-20 "${testcvs} log -r1.2: file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"

	  dotest log-21 "${testcvs} log -r:1.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-22 "${testcvs} log -r1.1:1.2 file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  cd ..
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	log2)
	  # More "cvs log" tests, for example the file description.

	  # Setting the file description doesn't yet work client/server, so 
	  # skip these tests for remote.
	  if test "x$remote" = xno; then

	  # Check in a file
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest log2-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 'first revision' > file1
	  dotest log2-2 "${testcvs} add -m file1-is-for-testing file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use '\''cvs commit'\'' to add this file permanently'
	  dotest log2-3 "${testcvs} -q commit -m 1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest log2-4 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
file1-is-for-testing
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
1
============================================================================="

	  cd ..
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir

	  fi # end of tests skipped for remote

	  ;;
	crerepos)
	  # Various tests relating to creating repositories, operating
	  # on repositories created with old versions of CVS, etc.

	  # Because this test is all about -d options and such, it
	  # at least to some extent needs to be different for remote vs.
	  # local.
	  if test "x$remote" = "xno"; then

	    # First, if the repository doesn't exist at all...
	    dotest_fail crerepos-1 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} \[[a-z]* aborted\]: ${TESTDIR}/crerepos/CVSROOT: .*"
	    mkdir crerepos

	    # The repository exists but CVSROOT doesn't.
	    dotest_fail crerepos-2 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} \[[a-z]* aborted\]: ${TESTDIR}/crerepos/CVSROOT: .*"
	    mkdir crerepos/CVSROOT

	    # Checkout of nonexistent module
	    dotest_fail crerepos-3 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} [a-z]*: cannot find module .cvs-sanity. - ignored"

	    # Now test that CVS works correctly without a modules file
	    # or any of that other stuff.  In particular, it *must*
	    # function if administrative files added to CVS recently (since
	    # CVS 1.3) do not exist, because the repository might have
	    # been created with an old version of CVS.
	    mkdir tmp; cd tmp
	    dotest crerepos-4 \
"${testcvs} -q -d ${TESTDIR}/crerepos co CVSROOT" \
''
	    if echo yes | \
${testcvs} -d ${TESTDIR}/crerepos release -d CVSROOT >>${LOGFILE}; then
	      pass crerepos-5
	    else
	      fail crerepos-5
	    fi
	    rm -r CVS
	    cd ..
	    # The directory tmp should be empty
	    dotest crerepos-6 "rmdir tmp" ''

	  else
	    # For remote, just create the repository.  We don't yet do
	    # the various other tests above for remote but that should be
	    # changed.
	    mkdir crerepos
	    mkdir crerepos/CVSROOT
	  fi

	  if test "x$remote" = "xno"; then
	    # Test that CVS rejects a relative path in CVSROOT.
	    mkdir 1; cd 1
	    dotest_fail crerepos-6a "${testcvs} -q -d ../crerepos get ." \
"${PROG} \[[a-z]* aborted\]: CVSROOT ../crerepos must be an absolute pathname"
	    cd ..
	    rm -r 1

	    mkdir 1; cd 1
	    dotest_fail crerepos-6b "${testcvs} -d crerepos init" \
"${PROG} \[[a-z]* aborted\]: CVSROOT crerepos must be an absolute pathname"
	    cd ..
	    rm -r 1
	  else # remote
	    # Test that CVS rejects a relative path in CVSROOT.
	    mkdir 1; cd 1
	    dotest_fail crerepos-6a \
"${testcvs} -q -d :ext:`hostname`:../crerepos get ." \
"Root ../crerepos must be an absolute pathname"
	    cd ..
	    rm -r 1

	    mkdir 1; cd 1
	    dotest_fail crerepos-6b \
"${testcvs} -d :ext:`hostname`:crerepos init" \
"Root crerepos must be an absolute pathname"
	    cd ..
	    rm -r 1
	  fi # end of tests to be skipped for remote

	  # CVS better not create a history file--if the administrator 
	  # doesn't need it and wants to save on disk space, they just
	  # delete it.
	  dotest_fail crerepos-7 \
"test -f ${TESTDIR}/crerepos/CVSROOT/history" ''

	  ;;

	rcs)
	  # Test ability to import an RCS file.  Note that this format
	  # is fixed--files written by RCS5, and other software which
	  # implements this format, will be out there "forever" and
	  # CVS must always be able to import such files.

	  # TODO: would be nice to have a corresponding test for exporting
	  # RCS files.  Rather than try to write a rigorous check for whether
	  # the file CVS exports is legal, we could just write a simpler test
	  # for what CVS actually exports, and revise the check as needed.

	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  # Currently the way to import an RCS file is to copy it
	  # directly into the repository.
	  # This file was written by RCS 5.7, and then the dates were
	  # hacked so that we test year 2000 stuff.  Note also that
	  # "author" names are just strings, as far as importing
	  # RCS files is concerned--they need not correspond to user
	  # IDs on any particular system.
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file1,v
head	1.3;
access;
symbols;
locks; strict;
comment	@# @;


1.3
date	2000.11.24.15.58.37;	author kingdon;	state Exp;
branches;
next	1.2;

1.2
date	96.11.24.15.57.41;	author kingdon;	state Exp;
branches;
next	1.1;

1.1
date	96.11.24.15.56.05;	author kingdon;	state Exp;
branches;
next	;


desc
@file1 is for testing CVS
@


1.3
log
@delete second line; modify twelfth line
@
text
@This is the first line
This is the third line
This is the fourth line
This is the fifth line
This is the sixth line
This is the seventh line
This is the eighth line
This is the ninth line
This is the tenth line
This is the eleventh line
This is the twelfth line (and what a line it is)
This is the thirteenth line
@


1.2
log
@add more lines
@
text
@a1 1
This is the second line
d11 1
a11 1
This is the twelfth line
@


1.1
log
@add file1
@
text
@d2 12
@
EOF
	  dotest rcs-1 "${testcvs} -q co first-dir" 'U first-dir/file1'
	  cd first-dir
	  dotest rcs-2 "${testcvs} -q log" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
file1 is for testing CVS
----------------------------
revision 1\.3
date: 2000/11/24 15:58:37;  author: kingdon;  state: Exp;  lines: ${PLUS}1 -2
delete second line; modify twelfth line
----------------------------
revision 1\.2
date: 1996/11/24 15:57:41;  author: kingdon;  state: Exp;  lines: ${PLUS}12 -0
add more lines
----------------------------
revision 1\.1
date: 1996/11/24 15:56:05;  author: kingdon;  state: Exp;
add file1
============================================================================="

	  # Note that the dates here are chosen so that (a) we test
	  # at least one date after 2000, (b) we will notice if the
	  # month and day are getting mixed up with each other.
	  # TODO: also test that year isn't getting mixed up with month
	  # or day, for example 01-02-03.

	  # ISO8601 format.  There are many, many, other variations
	  # specified by ISO8601 which we should be testing too.
	  dotest rcs-3 "${testcvs} -q log -d 1996-12-11<" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 3;	selected revisions: 1
description:
file1 is for testing CVS
----------------------------
revision 1\.3
date: 2000/11/24 15:58:37;  author: kingdon;  state: Exp;  lines: ${PLUS}1 -2
delete second line; modify twelfth line
============================================================================="

	  # RFC822 format (as amended by RFC1123).
	  if ${testcvs} -q log -d '<3 Apr 2000 00:00' >${TESTDIR}/rcs4.tmp
	  then
	    dotest rcs-4 "cat ${TESTDIR}/rcs4.tmp" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 3;	selected revisions: 2
description:
file1 is for testing CVS
----------------------------
revision 1\.2
date: 1996/11/24 15:57:41;  author: kingdon;  state: Exp;  lines: ${PLUS}12 -0
add more lines
----------------------------
revision 1\.1
date: 1996/11/24 15:56:05;  author: kingdon;  state: Exp;
add file1
============================================================================="
	  else
	    fail rcs-4
	  fi
	  cd ..

	  rm -r first-dir ${TESTDIR}/rcs4.tmp
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	big)

	  # Test ability to operate on big files.  Intention is to
	  # test various realloc'ing code in RCS_deltas, rcsgetkey,
	  # etc.  "big" is currently defined to be 1000 lines (64000
	  # bytes), which in terms of files that users will use is not
	  # large, merely average, but my reasoning is that this
	  # should be big enough to make sure realloc'ing is going on
	  # and that raising it a lot would start to stress resources
	  # on machines which run the tests, without any significant
	  # benefit.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest big-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  for i in 0 1 2 3 4 5 6 7 8 9; do
	    for j in 0 1 2 3 4 5 6 7 8 9; do
	      for k in 0 1 2 3 4 5 6 7 8 9; do
		echo \
"This is line ($i,$j,$k) which goes into the file file1 for testing" >>file1
	      done
	    done
	  done
	  dotest big-2 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest big-3 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..
	  rm -r first-dir
	  dotest big-4 "${testcvs} -q get first-dir" "U first-dir/file1"

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modes)
	  # Test repository permissions (CVSUMASK and so on).
	  # Although the tests in this section "cheat" by testing
	  # repository permissions, which are sort of not a user-visible
	  # sort of thing, the modes do have user-visible consequences,
	  # such as whether a second user can check out the files.  But
	  # it would be awkward to test the consequences, so we don't.

	  # Solaris /bin/sh doesn't support export -n.  I'm not sure
	  # what we can do about this, other than hope that whoever
	  # is running the tests doesn't have CVSUMASK set.
	  #export -n CVSUMASK # if unset, defaults to 002

	  umask 077
	  mkdir 1; cd 1
	  dotest modes-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest modes-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  dotest modes-3 "${testcvs} add aa" \
"${PROG} [a-z]*: scheduling file .aa. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest modes-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aa,v
done
Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  dotest modes-5 "ls -l ${TESTDIR}/cvsroot/first-dir/aa,v" \
"-r--r--r-- .*"

	  # Test for whether we can set the execute bit.
	  chmod +x aa
	  echo change it >>aa
	  dotest modes-6 "${testcvs} -q ci -m set-execute-bit" \
"Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done"
	  # If CVS let us update the execute bit, it would be set here.
	  # But it doesn't, and as far as I know that is longstanding
	  # CVS behavior.
	  dotest modes-7 "ls -l ${TESTDIR}/cvsroot/first-dir/aa,v" \
"-r--r--r-- .*"

	  # OK, now manually change the modes and see what happens.
	  chmod g=r,o= ${TESTDIR}/cvsroot/first-dir/aa,v
	  echo second line >>aa
	  dotest modes-7a "${testcvs} -q ci -m set-execute-bit" \
"Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest modes-7b "ls -l ${TESTDIR}/cvsroot/first-dir/aa,v" \
"-r--r----- .*"

	  CVSUMASK=007
	  export CVSUMASK
	  touch ab
	  # Might as well test the execute bit too.
	  chmod +x ab
	  dotest modes-8 "${testcvs} add ab" \
"${PROG} [a-z]*: scheduling file .ab. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest modes-9 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/ab,v
done
Checking in ab;
${TESTDIR}/cvsroot/first-dir/ab,v  <--  ab
initial revision: 1\.1
done"
	  if test "x$remote" = xyes; then
	    # The problem here is that the CVSUMASK environment variable
	    # needs to be set on the server (e.g. .bashrc).  This is, of
	    # course, bogus, but that is the way it is currently.
	    dotest modes-10 "ls -l ${TESTDIR}/cvsroot/first-dir/ab,v" \
"-r-xr-x---.*" "-r-xr-xr-x.*"
	  else
	    dotest modes-10 "ls -l ${TESTDIR}/cvsroot/first-dir/ab,v" \
"-r-xr-x---.*"
	  fi

	  # OK, now add a file on a branch.  Check that the mode gets
	  # set the same way (it is a different code path in CVS).
	  dotest modes-11 "${testcvs} -q tag -b br" 'T aa
T ab'
	  dotest modes-12 "${testcvs} -q update -r br" ''
	  touch ac
	  dotest modes-13 "${testcvs} add ac" \
"${PROG} [a-z]*: scheduling file .ac. for addition on branch .br.
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  # Not sure it really makes sense to refer to a "previous revision"
	  # when we are just now adding the file; as far as I know
	  # that is longstanding CVS behavior, for what it's worth.
	  dotest modes-14 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/ac,v
done
Checking in ac;
${TESTDIR}/cvsroot/first-dir/Attic/ac,v  <--  ac
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  if test "x$remote" = xyes; then
	    # The problem here is that the CVSUMASK environment variable
	    # needs to be set on the server (e.g. .bashrc).  This is, of
	    # course, bogus, but that is the way it is currently.
	    dotest modes-15 \
"ls -l ${TESTDIR}/cvsroot/first-dir/Attic/ac,v" \
"-r--r--r--.*"
	  else
	    dotest modes-15 \
"ls -l ${TESTDIR}/cvsroot/first-dir/Attic/ac,v" \
"-r--r-----.*"
	  fi

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  # Perhaps should restore the umask and CVSUMASK.  But the other
	  # tests "should" not care about them...
	  ;;

	stamps)
	  # Test timestamps.
	  mkdir 1; cd 1
	  dotest stamps-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest stamps-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  echo '$''Id$' >kw
	  ls -l aa >${TESTDIR}/1/stamp.aa.touch
	  ls -l kw >${TESTDIR}/1/stamp.kw.touch
	  # "sleep 1" would suffice if we could assume ls --full-time, but
	  # that is as far as I know unique to GNU ls.  Is there some POSIX.2
	  # way to get the timestamp of a file, including the seconds?
	  sleep 60
	  dotest stamps-3 "${testcvs} add aa kw" \
"${PROG} [a-z]*: scheduling file .aa. for addition
${PROG} [a-z]*: scheduling file .kw. for addition
${PROG} [a-z]*: use .cvs commit. to add these files permanently"
	  ls -l aa >${TESTDIR}/1/stamp.aa.add
	  ls -l kw >${TESTDIR}/1/stamp.kw.add
	  # "cvs add" should not muck with the timestamp.
	  dotest stamps-4aa \
"cmp ${TESTDIR}/1/stamp.aa.touch ${TESTDIR}/1/stamp.aa.add" ''
	  dotest stamps-4kw \
"cmp ${TESTDIR}/1/stamp.kw.touch ${TESTDIR}/1/stamp.kw.add" ''
	  sleep 60
	  dotest stamps-5 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aa,v
done
Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/kw,v
done
Checking in kw;
${TESTDIR}/cvsroot/first-dir/kw,v  <--  kw
initial revision: 1\.1
done"
	  ls -l aa >${TESTDIR}/1/stamp.aa.ci
	  ls -l kw >${TESTDIR}/1/stamp.kw.ci
	  # If there are no keywords, "cvs ci" leaves the timestamp alone
	  # If there are, it sets the timestamp to the date of the commit.
	  # I'm not sure how logical this is, but it is intentional.
	  # If we wanted to get fancy we would make sure the time as
	  # reported in "cvs log kw" matched stamp.kw.ci.  But that would
	  # be a lot of work.
	  dotest stamps-6aa \
	    "cmp ${TESTDIR}/1/stamp.aa.add ${TESTDIR}/1/stamp.aa.ci" ''
	  if cmp ${TESTDIR}/1/stamp.kw.add ${TESTDIR}/1/stamp.kw.ci >/dev/null
	  then
	    fail stamps-6kw
	  else
	    pass stamps-6kw
	  fi
	  cd ../..
	  sleep 60
	  mkdir 2
	  cd 2
	  dotest stamps-7 "${testcvs} -q get first-dir" "U first-dir/aa
U first-dir/kw"
	  cd first-dir
	  ls -l aa >${TESTDIR}/1/stamp.aa.get
	  ls -l kw >${TESTDIR}/1/stamp.kw.get
	  # On checkout, CVS should set the timestamp to the date that the
	  # file was committed.  Could check that the time as reported in
	  # "cvs log aa" matches stamp.aa.get, but that would be a lot of
	  # work.
	  if cmp ${TESTDIR}/1/stamp.aa.ci ${TESTDIR}/1/stamp.aa.get >/dev/null
	  then
	    fail stamps-8aa
	  else
	    pass stamps-8aa
	  fi
	  dotest stamps-8kw \
	    "cmp ${TESTDIR}/1/stamp.kw.ci ${TESTDIR}/1/stamp.kw.get" ''

	  # Now we want to see what "cvs update" does.
	  sleep 60
	  echo add a line >>aa
	  echo add a line >>kw
	  dotest stamps-9 "${testcvs} -q ci -m change-them" \
"Checking in aa;
${TESTDIR}/cvsroot/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done
Checking in kw;
${TESTDIR}/cvsroot/first-dir/kw,v  <--  kw
new revision: 1\.2; previous revision: 1\.1
done"
	  ls -l aa >${TESTDIR}/1/stamp.aa.ci2
	  ls -l kw >${TESTDIR}/1/stamp.kw.ci2
	  cd ../..
	  cd 1/first-dir
	  sleep 60
	  dotest stamps-10 "${testcvs} -q update" '[UP] aa
[UP] kw'
	  # this doesn't serve any function other than being able to
	  # look at it manually, as we have no machinery for dates being
	  # newer or older than other dates.
	  date >${TESTDIR}/1/stamp.debug.update
	  ls -l aa >${TESTDIR}/1/stamp.aa.update
	  ls -l kw >${TESTDIR}/1/stamp.kw.update
	  # stamp.aa.update and stamp.kw.update should both be approximately
	  # the same as stamp.debug.update.  Perhaps we could be testing
	  # this in a more fancy fashion by "touch stamp.before" before
	  # stamps-10, "touch stamp.after" after, and then using ls -t
	  # to check them.  But for now we just make sure that the *.update
	  # stamps differ from the *.ci2 ones.
	  # As for the rationale, this is so that if one updates and gets
	  # a new revision, then "make" will be sure to regard those files
	  # as newer than .o files which may be sitting around.
	  if cmp ${TESTDIR}/1/stamp.aa.update ${TESTDIR}/1/stamp.aa.ci2 \
	     >/dev/null
	  then
	    fail stamps-11aa
	  else
	    pass stamps-11aa
	  fi
	  if cmp ${TESTDIR}/1/stamp.kw.update ${TESTDIR}/1/stamp.kw.ci2 \
	     >/dev/null
	  then
	    fail stamps-11kw
	  else
	    pass stamps-11kw
	  fi

	  cd ../..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	sticky)
	  # More tests of sticky tags, particularly non-branch sticky tags.
	  mkdir 1; cd 1
	  dotest sticky-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest sticky-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir

	  touch file1
	  dotest sticky-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest sticky-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest sticky-5 "${testcvs} -q tag tag1" "T file1"
	  echo add a line >>file1
	  dotest sticky-6 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest sticky-7 "${testcvs} -q update -r tag1" "[UP] file1"
	  dotest sticky-8 "cat file1" ''
	  dotest sticky-9 "${testcvs} -q update" ''
	  dotest sticky-10 "cat file1" ''
	  touch file2
	  dotest_fail sticky-11 "${testcvs} add file2" \
"${PROG} [a-z]*: cannot add file on non-branch tag tag1"
	  dotest sticky-12 "${testcvs} -q update -A" "[UP] file1
${QUESTION} file2" "${QUESTION} file2
[UP] file1"
	  dotest sticky-13 "${testcvs} add file2" \
"${PROG} [a-z]*: scheduling file .file2. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	keyword)
	  # Test keyword expansion.
	  mkdir 1; cd 1
	  dotest keyword-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest keyword-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
          cd first-dir

	  echo '$''Author$' > file1
	  echo '$''Date$' >> file1
	  echo '$''Header$' >> file1
	  echo '$''Id$' >> file1
	  echo '$''Locker$' >> file1
	  echo '$''Name$' >> file1
	  echo '$''RCSfile$' >> file1
	  echo '$''Revision$' >> file1
	  echo '$''Source$' >> file1
	  echo '$''State$' >> file1
	  echo '$''Nonkey$' >> file1
	  # Omit the trailing dollar sign
	  echo '$''Date' >> file1
	  # Put two keywords on one line
	  echo '$''State$' '$''State$' >> file1
	  # Use a header for Log
	  echo 'xx $''Log$' >> file1

	  dotest keyword-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest keyword-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest keyword-5 "cat file1" \
'\$'"Author: ${username} "'\$'"
"'\$'"Date: [0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] "'\$'"
"'\$'"Header: ${TESTDIR}/cvsroot/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Id: file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Locker:  "'\$'"
"'\$'"Name:  "'\$'"
"'\$'"RCSfile: file1,v "'\$'"
"'\$'"Revision: 1\.1 "'\$'"
"'\$'"Source: ${TESTDIR}/cvsroot/first-dir/file1,v "'\$'"
"'\$'"State: Exp "'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State: Exp "'\$'" "'\$'"State: Exp "'\$'"
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  # Use cvs admin to lock the RCS file in order to check -kkvl
	  # vs. -kkv.  CVS does not normally lock RCS files, but some
	  # people use cvs admin to enforce reserved checkouts.
	  dotest keyword-6 "${testcvs} admin -l file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
1\.1 locked
done"

	  dotest keyword-7 "${testcvs} update -kkv file1" "U file1"
	  dotest keyword-8 "cat file1" \
'\$'"Author: ${username} "'\$'"
"'\$'"Date: [0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] "'\$'"
"'\$'"Header: ${TESTDIR}/cvsroot/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Id: file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Locker:  "'\$'"
"'\$'"Name:  "'\$'"
"'\$'"RCSfile: file1,v "'\$'"
"'\$'"Revision: 1\.1 "'\$'"
"'\$'"Source: ${TESTDIR}/cvsroot/first-dir/file1,v "'\$'"
"'\$'"State: Exp "'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State: Exp "'\$'" "'\$'"State: Exp "'\$'"
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  dotest keyword-9 "${testcvs} update -kkvl file1" "U file1"
	  dotest keyword-10 "cat file1" \
'\$'"Author: ${username} "'\$'"
"'\$'"Date: [0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] "'\$'"
"'\$'"Header: ${TESTDIR}/cvsroot/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp ${username} "'\$'"
"'\$'"Id: file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp ${username} "'\$'"
"'\$'"Locker: ${username} "'\$'"
"'\$'"Name:  "'\$'"
"'\$'"RCSfile: file1,v "'\$'"
"'\$'"Revision: 1\.1 "'\$'"
"'\$'"Source: ${TESTDIR}/cvsroot/first-dir/file1,v "'\$'"
"'\$'"State: Exp "'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State: Exp "'\$'" "'\$'"State: Exp "'\$'"
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  dotest keyword-11 "${testcvs} update -kk file1" "U file1"
	  dotest keyword-12 "cat file1" \
'\$'"Author"'\$'"
"'\$'"Date"'\$'"
"'\$'"Header"'\$'"
"'\$'"Id"'\$'"
"'\$'"Locker"'\$'"
"'\$'"Name"'\$'"
"'\$'"RCSfile"'\$'"
"'\$'"Revision"'\$'"
"'\$'"Source"'\$'"
"'\$'"State"'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State"'\$'" "'\$'"State"'\$'"
xx "'\$'"Log"'\$'"
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  dotest keyword-13 "${testcvs} update -kv file1" "U file1"
	  dotest keyword-14 "cat file1" \
"${username}
[0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]
${TESTDIR}/cvsroot/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp
file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp


file1,v
1\.1
${TESTDIR}/cvsroot/first-dir/file1,v
Exp
"'\$'"Nonkey"'\$'"
"'\$'"Date
Exp Exp
xx file1,v
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  dotest keyword-15 "${testcvs} update -ko file1" "U file1"
	  dotest keyword-16 "cat file1" \
'\$'"Author"'\$'"
"'\$'"Date"'\$'"
"'\$'"Header"'\$'"
"'\$'"Id"'\$'"
"'\$'"Locker"'\$'"
"'\$'"Name"'\$'"
"'\$'"RCSfile"'\$'"
"'\$'"Revision"'\$'"
"'\$'"Source"'\$'"
"'\$'"State"'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State"'\$'" "'\$'"State"'\$'"
xx "'\$'"Log"'\$'

	  # Test the Name keyword.  First go back to normal expansion.

	  # FIXME: When using remote, update -A does not revert the
	  # keyword expansion mode.  We work around that bug here.
	  # This workaround should be removed when the bug is fixed.
	  if test "x$remote" = "xyes"; then
	    cd ..
	    rm -r first-dir
	    dotest keyword-17 "${testcvs} -q co first-dir" "U first-dir/file1"
	    cd first-dir
	  else
	    dotest keyword-17 "${testcvs} update -A file1" "U file1"
	  fi

	  echo '$''Name$' > file1
	  dotest keyword-18 "${testcvs} ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest keyword-19 "${testcvs} -q tag tag1" "T file1"
	  echo "change" >> file1
	  dotest keyword-20 "${testcvs} -q ci -m mod2 file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest keyword-21 "${testcvs} -q update -r tag1" "[UP] file1"

	  # FIXME: This test fails when remote.  The second expect
	  # string below should be removed when this is fixed.
	  dotest keyword-22 "cat file1" '\$'"Name: tag1 "'\$' \
'\$'"Name:  "'\$'

	  dotest keyword-23 "${testcvs} update -A file1" "[UP] file1"

	  # Test the Log keyword.
	  echo 'xx $''Log$' > file1
	  cat >${TESTDIR}/comment.tmp <<EOF
First log line
Second log line
EOF
	  dotest keyword-24 "${testcvs} ci -F ${TESTDIR}/comment.tmp file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"
	  rm -f ${TESTDIR}/comment.tmp
	  dotest keyword-25 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"

	  echo "change" >> file1
	  dotest keyword-26 "${testcvs} ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest keyword-27 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.5  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	toplevel)
	  # test the feature that cvs creates a CVS subdir also for
	  # the toplevel directory

	  # Some test, somewhere, is creating Emptydir.  That test
	  # should, perhaps, clean up for itself, but I don't know which
	  # one it is.
	  rm -rf ${CVSROOT_DIRNAME}/CVSROOT/Emptydir

	  mkdir 1; cd 1
	  dotest toplevel-1 "${testcvs} -q co -l ." ''
	  mkdir top-dir
	  dotest toplevel-2 "${testcvs} add top-dir" \
"Directory ${TESTDIR}/cvsroot/top-dir added to the repository"
	  cd top-dir

	  touch file1
	  dotest toplevel-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .cvs commit. to add this file permanently"
	  dotest toplevel-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/top-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/top-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  cd ../..
	  rm -r 1; mkdir 1; cd 1
	  dotest toplevel-5 "${testcvs} co top-dir" \
"${PROG} [a-z]*: Updating top-dir
U top-dir/file1"

	  dotest toplevel-6 "${testcvs} update top-dir" \
"${PROG} [a-z]*: Updating top-dir"
	  dotest toplevel-7 "${testcvs} update"  \
"${PROG} [a-z]*: Updating \.
${PROG} [a-z]*: Updating top-dir"

	  dotest toplevel-8 "${testcvs} update -d top-dir" \
"${PROG} [a-z]*: Updating top-dir"
	  # FIXME: This test fails in cvs starting from 1.9.2 because
	  # it updates "file1" in "1".  Test modules3-7f also finds
	  # (and tolerates) this bug.  The second expect string below
	  # should be removed when this is fixed.  The first expect
	  # string is the behavior of remote CVS.  There is some sentiment
	  # that
	  #   "${PROG} [a-z]*: Updating \.
          #   ${PROG} [a-z]*: Updating top-dir"
	  # is correct but it isn't clear why that would be correct instead
	  # of the remote CVS behavior.
	  #
	  # The DOTSTAR matches of a bunch of lines like
	  # "U CVSROOT/checkoutlist".  Trying to match them more precisely
	  # seemed to cause trouble.  For example CVSROOT/cvsignore will
	  # be present or absent depending on whether we ran the "ignore"
	  # test or not.
	  dotest toplevel-9 "${testcvs} update -d" \
"${PROG} [a-z]*: Updating \.
${PROG} [a-z]*: Updating CVSROOT
${DOTSTAR}
${PROG} [a-z]*: Updating top-dir" \
"${PROG} [a-z]*: Updating \.
U file1
${PROG} [a-z]*: Updating top-dir"

	  cd ..
	  rm -r 1
	  ;;

	*)
	   echo $what is not the name of a test -- ignored
	   ;;
	esac
done

echo "OK, all tests completed."

# TODO:
# * use "test" not "[" and see if all test's support `-z'
# * Test `cvs update -d foo' (where foo does not exist).
# * Test `cvs update foo bar' (where foo and bar are both from the
#   same directory in the repository).  Suppose one is a branch--make
#   sure that both directories get updated with the respective correct
#   thing.
# * `cvs update ../foo'.  Also ../../foo ./../foo foo/../../bar /foo/bar
#   foo/.././../bar foo/../bar etc.
# * Test all flags in modules file.
#   Test that ciprog gets run both on checkin in that directory, or a
#     higher-level checkin which recurses into it.
# * More tests of keyword expansion.
# * Test operations on a directory that contains other directories but has
#   no files of its own.
# * -t global option
# * cvs rm followed by cvs add or vice versa (with no checkin in between).
# * cvs rm twice (should be a nice error message).
# * -P option to checkout--(a) refrains from checking out new empty dirs,
#   (b) prunes empty dirs already there.
# * Test that cvs -d `hostname`:${TESTDIR}/non/existent co foo
#   gives an appropriate error (e.g.
#     Cannot access ${TESTDIR}/non-existent/CVSROOT
#     No such file or directory).
#   (like basica-9, but for remote).
# * Test ability to send notifications in response to watches.  (currently
#   hard to test because CVS doesn't send notifications if username is the
#   same).
# * Test that remote edit and/or unedit works when disconnected from
#   server (e.g. set CVS_SERVER to "foobar").
# * Test things to do with the CVS/* files, esp. CVS/Root....
# End of TODO list.

# Remove the test directory, but first change out of it.
cd /tmp
rm -rf ${TESTDIR}

# end of sanity.sh
