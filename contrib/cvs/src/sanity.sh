#! /bin/sh
:
#	sanity.sh -- a growing testsuite for cvs.
#
# The copyright notice said: "Copyright (C) 1992, 1993 Cygnus Support"
# I'm not adding new copyright notices for new years as our recent 
# practice has been to include copying terms without copyright notices.
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
# Original Author: K. Richard Pixley

# usage: sanity.sh [-r] @var{cvs-to-test} @var{tests-to-run}
# -r means to test remote instead of local cvs.
# @var{tests-to-run} are the names of the tests to run; if omitted run all
# tests.

# See TODO list at end of file.

# You can't run CVS as root; print a nice error message here instead
# of somewhere later, after making a mess.
# Commented out because:
# (1) whoami is not portable.  If memory serves the POSIX way is "id -un".
#     ("logname" or "who am i" are similar but different--they have more to
#      do with who logged in on your tty than your uid).
# (2) This definition of "root" doesn't quite match CVS's (which is based
#     on uid 0, not username "root").
#case "`whoami`" in
#  "root" )
#    echo "sanity.sh: test suite does not work correctly when run as root" >&2
#    exit 1
#  ;;
#esac

# required to make this script work properly.
unset CVSREAD

# The default value of /tmp/cvs-sanity for TESTDIR is dubious,
# because it loses if two people/scripts try to run the tests
# at the same time.  Some possible solutions:
# 1.  Use /tmp/cvs-test$$.  One disadvantage is that the old
#     cvs-test* directories would pile up, because they wouldn't
#     necessarily get removed.
# 2.  Have everyone/everything running the testsuite set
#     TESTDIR to some appropriate directory.
# 3.  Have the default value of TESTDIR be some variation of
#     `pwd`/cvs-sanity.  The biggest problem here is that we have
#     been fairly careful to test that CVS prints in messages the
#     actual pathnames that we pass to it, rather than a different
#     pathname for the same directory, as may come out of `pwd`.
#     So this would be lost if everything was `pwd`-based.  I suppose
#     if we wanted to get baroque we could start making symlinks
#     to ensure the two are different.
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
username="[-a-zA-Z0-9][-a-zA-Z0-9]*"

# Regexp to match the name of a temporary file (from cvs_temp_name).
# This appears in certain diff output.
tempname="[-a-zA-Z0-9/.%_]*"

# On cygwin32, we may not have /bin/sh.
if [ -r /bin/sh ]; then
  TESTSHELL="/bin/sh"
else
  TESTSHELL=`type -p sh 2>/dev/null`
  if [ ! -r "$TESTSHELL" ]; then
    TESTSHELL="/bin/sh"
  fi
fi

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

# Like dotest except output is sorted.
dotest_sort ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
  if $2 >${TESTDIR}/dotest.tmp1 2>&1; then
    : so far so good
  else
    status=$?
    cat ${TESTDIR}/dotest.tmp1 >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  sort < ${TESTDIR}/dotest.tmp1 > ${TESTDIR}/dotest.tmp
  dotest_internal "$@"
}

# clean any old remnants
rm -rf ${TESTDIR}
mkdir ${TESTDIR}
cd ${TESTDIR}
# This will show up in cvs history output where it prints the working
# directory.  It should *not* appear in any cvs output referring to the
# repository; cvs should use the name of the repository as specified.
#
# Note that using pwd here rather than /bin/pwd will make it even less
# likely that we test whether CVS is distinguishing between TMPPWD
# and TESTDIR.  However, there is no guarantee that will test it anyway.
# If we really care, we should do something along the lines of:
#   cd /tmp/cvs-sanity  # In reality, overridable with environment variable?
#   mkdir realdir
#   ln -s realdir testdir
#   TESTDIR=/tmp/cvs-sanity/testdir
#   TMPPWD=/tmp/cvs-sanity/realdir
TMPPWD=`pwd`

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
	tests="basica basicb basicc basic1 deep basic2"
	tests="${tests} rdiff death death2 branches"
	tests="${tests} rcslib multibranch import importb join join2 join3"
	tests="${tests} new newb conflicts conflicts2 conflicts3"
	tests="${tests} modules modules2 modules3 mflag editor errmsg1 errmsg2"
	tests="${tests} devcom devcom2 devcom3 watch4"
	tests="${tests} ignore binfiles binfiles2 mcopy binwrap binwrap2"
	tests="${tests} binwrap3 mwrap info config"
	tests="${tests} serverpatch log log2 ann crerepos rcs big modes stamps"
	tests="${tests} sticky keyword keywordlog"
	tests="${tests} toplevel head tagdate multibranch2"
	tests="${tests} admin reserved"
	tests="${tests} cvsadm diffmerge1 diffmerge2"
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
"${PROG} [a-z]*: use .${PROG} add. to create an entry for ssfile
${PROG}"' \[[a-z]* aborted\]: correct above errors first!' \
"${PROG}"' [a-z]*: nothing known about `ssfile'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'

	  dotest basica-4 "${testcvs} add ssfile" \
"${PROG}"' [a-z]*: scheduling file `ssfile'\'' for addition
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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

	  # As long as we have a file with a few revisions, test
	  # a few "cvs admin -o" invocations.
	  cd sdir/ssdir
	  dotest_fail basica-o1 "${testcvs} admin -o 1.2::1.2" \
"${PROG} [a-z]*: while processing more than one file:
${PROG} \[[a-z]* aborted\]: attempt to specify a numeric revision"
	  dotest basica-o2 "${testcvs} admin -o 1.2::1.2 ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o2a "${testcvs} admin -o 1.1::NOT_RESERVED ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest_fail basica-o2b "${testcvs} admin -o 1.1::NOT_EXIST ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
${PROG} [a-z]*: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v: Revision NOT_EXIST doesn't exist.
${PROG} [a-z]*: cannot modify RCS file for .ssfile."
	  dotest basica-o3 "${testcvs} admin -o 1.2::1.3 ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o4 "${testcvs} admin -o 3.1:: ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o5 "${testcvs} admin -o ::1.1 ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o5a "${testcvs} -n admin -o 1.2::3.1 ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
deleting revision 2\.0
deleting revision 1\.3
done"
	  dotest basica-o6 "${testcvs} admin -o 1.2::3.1 ssfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
deleting revision 2\.0
deleting revision 1\.3
done"
	  dotest basica-o7 "${testcvs} log -N ssfile" "
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir/ssdir/ssfile,v
Working file: ssfile
head: 3\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 3\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}0 -0
bump-it
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
modify-it
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add-it
============================================================================="
	  dotest basica-o8 "${testcvs} -q update -p -r 1.1 ssfile" "ssfile"
	  cd ../..

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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest basicb-0c "${testcvs} -q ci -m add-it topfile" \
"RCS file: ${TESTDIR}/cvsroot/topfile,v
done
Checking in topfile;
${TESTDIR}/cvsroot/topfile,v  <--  topfile
initial revision: 1\.1
done"
	  cd ..
	  rm -r 1
	  mkdir 2; cd 2
	  dotest basicb-0d "${testcvs} -q co -l ." "U topfile"
	  # Now test the ability to run checkout on an existing working
	  # directory without having it lose its mind.  I don't know
	  # whether this is tested elsewhere in sanity.sh.  A more elaborate
	  # test might also have modified files, make sure it works if
	  # the modules file was modified to add new directories to the
	  # module, and such.
	  dotest basicb-0d0 "${testcvs} -q co -l ." ""
	  mkdir first-dir
	  dotest basicb-0e "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd ..
	  rm -r 2

:	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest basicb-1 "${testcvs} -q co first-dir" ''
	  dotest basicb-1a "test -d CVS" ''

	  # In 1b and 1c, the first string matches if we're using absolute
	  # paths, while the second matches if RELATIVE_REPOS is defined
	  # (we're using relative paths).

	  dotest basicb-1b "cat CVS/Repository" \
"${TESTDIR}/cvsroot/\." \
"\."
	  dotest basicb-1c "cat first-dir/CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir" \
"first-dir"

	  cd first-dir
	  # Note that the name Emptydir is chosen to test that CVS just
	  # treats it like any other directory name.  It should be
	  # special only when it is directly in $CVSROOT/CVSROOT.
	  mkdir Emptydir sdir2
	  dotest basicb-2 "${testcvs} add Emptydir sdir2" \
"Directory ${TESTDIR}/cvsroot/first-dir/Emptydir added to the repository
Directory ${TESTDIR}/cvsroot/first-dir/sdir2 added to the repository"
	  cd Emptydir
	  echo sfile1 starts >sfile1
	  dotest basicb-2a10 "${testcvs} -n add sfile1" \
"${PROG} [a-z]*: scheduling file .sfile1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest basicb-2a11 "${testcvs} status sfile1" \
"${PROG} [a-z]*: use .${PROG} add. to create an entry for sfile1
===================================================================
File: sfile1           	Status: Unknown

   Working revision:	No entry for sfile1
   Repository revision:	No revision control file"
	  dotest basicb-3 "${testcvs} add sfile1" \
"${PROG} [a-z]*: scheduling file .sfile1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest basicb-4a "${testcvs} -q ci CVS" \
"${PROG} [a-z]*: warning: directory CVS specified in argument
${PROG} [a-z]*: but CVS uses CVS for its own purposes; skipping CVS directory"
	  cd ..
	  dotest basicb-5 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Emptydir/sfile1,v
done
Checking in Emptydir/sfile1;
${TESTDIR}/cvsroot/first-dir/Emptydir/sfile1,v  <--  sfile1
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir2/sfile2,v
done
Checking in sdir2/sfile2;
${TESTDIR}/cvsroot/first-dir/sdir2/sfile2,v  <--  sfile2
initial revision: 1\.1
done"
	  echo sfile1 develops >Emptydir/sfile1
	  dotest basicb-6 "${testcvs} -q ci -m modify" \
"Checking in Emptydir/sfile1;
${TESTDIR}/cvsroot/first-dir/Emptydir/sfile1,v  <--  sfile1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest basicb-7 "${testcvs} -q tag release-1" 'T Emptydir/sfile1
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
'U first-dir1/Emptydir/sfile1
U first-dir1/sdir2/sfile2'
	  rm -r first-dir1

	  rm -r first-dir

	  # FIXME? basicb-9 used to check things out like this:
	  #   U newdir/Emptydir/sfile1
	  #   U newdir/sdir2/sfile2
	  # but that's difficult to do.  The whole "shorten" thing
	  # is pretty bogus, because it will break on things
	  # like "cvs co foo/bar baz/quux".  Unless there's some
	  # pretty detailed expansion and analysis of the command-line
	  # arguments, we shouldn't do "shorten" stuff at all.

	  dotest basicb-9 \
"${testcvs} -q co -d newdir -r release-1 first-dir/Emptydir first-dir/sdir2" \
'U newdir/first-dir/Emptydir/sfile1
U newdir/first-dir/sdir2/sfile2'
	  dotest basicb-9a "test -d CVS" ''

	  # In 9b through 9f, the first string matches if we're using
          # absolute paths, while the second matches if RELATIVE_REPOS
	  # is defined (we're using relative paths).

	  dotest basicb-9b "cat CVS/Repository" \
"${TESTDIR}/cvsroot/\." \
"\."
	  dotest basicb-9c "cat newdir/CVS/Repository" \
"${TESTDIR}/cvsroot/\." \
"\."
	  dotest basicb-9d "cat newdir/first-dir/CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir" \
"first-dir"
	  dotest basicb-9e "cat newdir/first-dir/Emptydir/CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir/Emptydir" \
"first-dir/Emptydir"
	  dotest basicb-9f "cat newdir/first-dir/sdir2/CVS/Repository" \
"${TESTDIR}/cvsroot/first-dir/sdir2" \
"first-dir/sdir2"

	  dotest basicb-10 "cat newdir/first-dir/Emptydir/sfile1 newdir/first-dir/sdir2/sfile2" \
"sfile1 develops
sfile2 starts"

	  rm -r newdir

	  # Hmm, this might be a case for CVSNULLREPOS, but CVS doesn't
	  # seem to deal with it...
	  if false; then
	  dotest basicb-11 "${testcvs} -q co -d sub1/sub2 first-dir" \
"U sub1/sub2/Emptydir/sfile1
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest basicb-17 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/second-dir/aa,v
done
Checking in aa;
${TESTDIR}/cvsroot/second-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  # Let's see if we can add something to Emptydir.
	  dotest basicb-18 "${testcvs} -q co -d t2/t3 first-dir second-dir" \
"U t2/t3/first-dir/Emptydir/sfile1
U t2/t3/first-dir/sdir2/sfile2
U t2/t3/second-dir/aa"
	  cd t2
	  touch emptyfile
	  # The fact that CVS lets us add a file here is a CVS bug, right?
	  # I can just make this an error message (on the add and/or the
	  # commit) without getting flamed, right?
	  # Right?
	  # Right?
	  dotest basicb-19 "${testcvs} add emptyfile" \
"${PROG} [a-z]*: scheduling file .emptyfile. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest basicb-20 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/CVSROOT/Emptydir/emptyfile,v
done
Checking in emptyfile;
${TESTDIR}/cvsroot/CVSROOT/Emptydir/emptyfile,v  <--  emptyfile
initial revision: 1\.1
done"
	  cd ..
	  rm -r t2

	  mkdir 1; cd 1
	  # Note that -H is an illegal option.
	  # I suspect that the choice between "illegal" and "invalid"
	  # depends on the user's environment variables, the phase
	  # of the moon (weirdness with optind), and who knows what else.
	  # I've been seeing "illegal"...
	  dotest_fail basicb-21 "${testcvs} -q admin -H" \
"admin: illegal option -- H
${PROG} \[admin aborted\]: specify ${PROG} -H admin for usage information" \
"admin: invalid option -- H
${PROG} \[admin aborted\]: specify ${PROG} -H admin for usage information"
	  cd ..
	  rmdir 1

	  # OK, while we have an Emptydir around, test a few obscure
	  # things about it.
	  mkdir edir; cd edir
	  dotest basicb-edir-1 "${testcvs} -q co -l CVSROOT" \
"U CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  dotest_fail basicb-edir-2 "test -d Emptydir" ''
	  # This tests the code in find_dirs which skips Emptydir.
	  dotest basicb-edir-3 "${testcvs} -q -n update -d -P" ''
	  cd ../..
	  rm -r edir

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -rf ${CVSROOT_DIRNAME}/second-dir
	  rm -rf ${CVSROOT_DIRNAME}/CVSROOT/Emptydir
	  rm -f ${CVSROOT_DIRNAME}/topfile,v
	  ;;

	basicc)
	  # More tests of basic/miscellaneous functionality.
	  mkdir 1; cd 1
	  dotest_fail basicc-1 "${testcvs} diff" \
"${PROG} [a-z]*: in directory \.:
${PROG} \[[a-z]* aborted\]: there is no version here; run .${PROG} checkout. first"
	  dotest basicc-2 "${testcvs} -q co -l ." ''
	  mkdir first-dir second-dir
	  dotest basicc-3 "${testcvs} add first-dir second-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository
Directory ${TESTDIR}/cvsroot/second-dir added to the repository"
	  # Old versions of CVS often didn't create this top-level CVS
	  # directory in the first place.  I think that maybe the only
	  # way to get avoid it currently is to let CVS create it, and
	  # then blow it away.  But that is perfectly legal; people who
	  # are used to the old behavior especially may be interested.
	  rm -r CVS
	  dotest basicc-4 "echo *" "first-dir second-dir"
	  dotest basicc-5 "${testcvs} update" \
"${PROG} [a-z]*: Updating first-dir
${PROG} [a-z]*: Updating second-dir" \
"${PROG} [a-z]*: Updating \.
${PROG} [a-z]*: Updating first-dir
${PROG} [a-z]*: Updating second-dir"

	  cd ..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
	  dotest deep-4b0a "${testcvs} -q diff" ''
	  dotest deep-4b0b "${testcvs} -q ci" ''
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
				    pass 29-$i
				else
				    fail 29-$i
				fi
			fi

			cd $i

			for j in file6 file7; do
				echo $j > $j
			done

			if ${CVS} add file6 file7  2>> ${LOGFILE}; then
			    pass 30-$i-$j
			else
			    fail 30-$i-$j
			fi
		done
		cd ../../..
		if ${CVS} update first-dir  ; then
		    pass 31
		else
		    fail 31
		fi

		# fixme: doesn't work right for added files.
		if ${CVS} log first-dir  >> ${LOGFILE}; then
		    pass 32
		else
		    fail 32
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
		    pass 33
		else
		    fail 33
		fi

# XXX why is this commented out???
#		if ${CVS} diff -u first-dir   >> ${LOGFILE} || test $? = 1 ; then
#		    pass 34
#		else
#		    fail 34
#		fi

		if ${CVS} ci -m "second dive" first-dir  >> ${LOGFILE} 2>&1; then
		    pass 35
		else
		    fail 35
		fi

		if ${CVS} tag second-dive first-dir  ; then
		    pass 36
		else
		    fail 36
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
			    pass 37-$i
			else
			    fail 37-$i
			fi

			# and add a new file
			echo file14 >file14

			if ${CVS} add file14  2>> ${LOGFILE}; then
			    pass 38-$i
			else
			    fail 38-$i
			fi
		done
		cd ../../..
		if ${CVS} update first-dir  ; then
		    pass 39
		else
		    fail 39
		fi

		# FIXME: doesn't work right for added files
		if ${CVS} log first-dir  >> ${LOGFILE}; then
		    pass 40
		else
		    fail 40
		fi

		if ${CVS} status first-dir  >> ${LOGFILE}; then
		    pass 41
		else
		    fail 41
		fi

# XXX why is this commented out?
#		if ${CVS} diff -u first-dir  >> ${LOGFILE} || test $? = 1 ; then
#		    pass 42
#		else
#		    fail 42
#		fi

		if ${CVS} ci -m "third dive" first-dir  >>${LOGFILE} 2>&1; then
		    pass 43
		else
		    fail 43
		fi
		dotest 43.5 "${testcvs} -q update first-dir" ''

		if ${CVS} tag third-dive first-dir  ; then
		    pass 44
		else
		    fail 44
		fi

		if echo "yes" | ${CVS} release -d first-dir  ; then
		    pass 45
		else
		    fail 45
		fi

		# end of third dive
		if test -d first-dir ; then
		    fail 45.5
		else
		    pass 45.5
		fi

		# now try some rtags

		# rtag HEADS
		if ${CVS} rtag rtagged-by-head first-dir  ; then
		    pass 46
		else
		    fail 46
		fi

		# tag by tag
		if ${CVS} rtag -r rtagged-by-head rtagged-by-tag first-dir  ; then
		    pass 47
		else
		    fail 47
		fi

		# tag by revision
		if ${CVS} rtag -r1.1 rtagged-by-revision first-dir  ; then
		    pass 48
		else
		    fail 48
		fi

		# rdiff by revision
		if ${CVS} rdiff -r1.1 -rrtagged-by-head first-dir  >> ${LOGFILE} || test $? = 1 ; then
		    pass 49
		else
		    fail 49
		fi

		# now export by rtagged-by-head and rtagged-by-tag and compare.
		if ${CVS} export -r rtagged-by-head first-dir  ; then
		    pass 50
		else
		    fail 50
		fi

		mv first-dir 1dir
		if ${CVS} export -r rtagged-by-tag first-dir  ; then
		    pass 51
		else
		    fail 51
		fi

		directory_cmp 1dir first-dir

		if $ISDIFF ; then
		    fail 52
		else
		    pass 52
		fi
		rm -r 1dir first-dir

		# checkout by revision vs export by rtagged-by-revision and compare.
		if ${CVS} export -rrtagged-by-revision -d export-dir first-dir  ; then
		    pass 53
		else
		    fail 53
		fi

		if ${CVS} co -r1.1 first-dir  ; then
		    pass 54
		else
		    fail 54
		fi

		# directory copies are done in an oblique way in order to avoid a bug in sun's tmp filesystem.
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - . | (cd ../first-dir.cpy ; tar xf -))

		directory_cmp first-dir export-dir

		if $ISDIFF ; then
		    fail 55
		else
		    pass 55
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
		    pass 57
		else
		    fail 57
		fi

		directory_cmp first-dir second-dir

		if $ISDIFF ; then
		    fail 58
		else
		    pass 58
		fi

		rm -r second-dir

		rm -r export-dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - . | (cd ../first-dir ; tar xf -))

		# update the top, cancelling sticky tags, retag, update other copy, compare.
		cd first-dir
		if ${CVS} update -A -l *file*  2>> ${LOGFILE}; then
		    pass 59
		else
		    fail 59
		fi

		# If we don't delete the tag first, cvs won't retag it.
		# This would appear to be a feature.
		if ${CVS} tag -l -d rtagged-by-revision  ; then
		    pass 60a
		else
		    fail 60a
		fi
		if ${CVS} tag -l rtagged-by-revision  ; then
		    pass 60b
		else
		    fail 60b
		fi

		cd ..
		mv first-dir 1dir
		mv first-dir.cpy first-dir
		cd first-dir

		dotest 61 "${testcvs} -q diff -u" ''

		if ${CVS} update  ; then
		    pass 62
		else
		    fail 62
		fi

		cd ..

		#### FIXME: is this expected to work???  Need to investigate
		#### and fix or remove the test.
#		directory_cmp 1dir first-dir
#
#		if $ISDIFF ; then
#		    fail 63
#		else
#		    pass 63
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
"O [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir           =first-dir= ${TMPPWD}/\*
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir           == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir           == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1      == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1      == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1/dir2 == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1/dir2 == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir           == ${TMPPWD}
M [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1      == ${TMPPWD}
M [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1      == ${TMPPWD}
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1/dir2 == ${TMPPWD}
M [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1/dir2 == ${TMPPWD}
F [0-9/]* [0-9:]* ${PLUS}0000 ${username}                     =first-dir= ${TMPPWD}/\*
T [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-revision:1\.1\]
O [0-9/]* [0-9:]* ${PLUS}0000 ${username} \[1\.1\] first-dir           =first-dir= ${TMPPWD}/\*
U [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == ${TMPPWD}/first-dir
U [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir           == ${TMPPWD}/first-dir" \
"O [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir           =first-dir= <remote>/\*
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir           == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1/dir2 == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir           == <remote>
M [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1      == <remote>
M [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1      == <remote>
A [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1/dir2 == <remote>
M [0-9/]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1/dir2 == <remote>
F [0-9/]* [0-9:]* ${PLUS}0000 ${username}                     =first-dir= <remote>/\*
T [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-head:A\]
T [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9/]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-revision:1\.1\]
O [0-9/]* [0-9:]* ${PLUS}0000 ${username} \[1\.1\] first-dir           =first-dir= <remote>/\*"

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
		dotest_sort rdiff-1 \
		  "${testcvs} import -I ! -m test-import-with-keyword trdiff TRDIFF T1" \
'

N trdiff/bar
N trdiff/foo
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
		    pass 65
		else
		    fail 65
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to remove this file permanently'
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
		    pass 66
		else
		    fail 66
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
		    pass 67
		else
		    fail 67
		fi

		# remove
		rm file1
		if ${CVS} rm file1  2>> ${LOGFILE}; then
		    pass 68
		else
		    fail 68
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
		    pass 69
		else
		    fail 69
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
		    pass 70
		else
		    fail 70
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
		    pass 71
		else
		    fail 71
		fi

		# log
		if ${CVS} log file1  >> ${LOGFILE}; then
		    pass 72
		else
		    fail 72
		fi

		# file4 will be dead at the time of branching and stay dead.
		echo file4 > file4
		dotest death-file4-add "${testcvs} add file4" \
"${PROG}"' [a-z]*: scheduling file `file4'\'' for addition
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to remove this file permanently'
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
		    pass 73
		else
		    fail 73
		fi

		# and move to the branch.
		if ${CVS} update -r branch1  ; then
		    pass 74
		else
		    fail 74
		fi

		dotest_fail death-file4-3 "test -f file4" ''

		# add a file in the branch
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
		    pass 75
		else
		    fail 75
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
		    pass 76
		else
		    fail 76
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
		    pass 77
		else
		    fail 77
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE} ; then
		    pass 78
		else
		    fail 78
		fi

		# add again
		echo line1 from branch1 >> file3
		if ${CVS} add file3  2>> ${LOGFILE}; then
		    pass 79
		else
		    fail 79
		fi

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
		    pass 80
		else
		    fail 80
		fi

		# change the first file
		echo line2 from branch1 >> file1

		# commit
		if ${CVS} ci -m test  >> ${LOGFILE} 2>&1; then
		    pass 81
		else
		    fail 81
		fi

		# remove the second
		rm file2
		if ${CVS} rm file2  2>> ${LOGFILE}; then
		    pass 82
		else
		    fail 82
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
		    pass 83
		else
		    fail 83
		fi

		# back to the trunk.
		if ${CVS} update -A  2>> ${LOGFILE}; then
		    pass 84
		else
		    fail 84
		fi

		dotest_fail death-file4-4 "test -f file4" ''

		if test -f file3 ; then
		    fail 85
		else
		    pass 85
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
		    pass 87
		else
		    fail 87
		fi

		# Make sure that we joined the correct change to file1
		if echo line2 from branch1 | cmp - file1 >/dev/null; then
		    pass 87a
		else
		    fail 87a
		fi

		# update
		if ${CVS} update  ; then
		    pass 88
		else
		    fail 88
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
		    pass 90
		else
		    fail 90
		fi

		# commit
		if ${CVS} ci -m test  >>${LOGFILE}; then
		    pass 91
		else
		    fail 91
		fi

		if test -f file1 ; then
		    fail 92
		else
		    pass 92
		fi

		# typo; try to get to the branch and fail
		dotest_fail 92.1a "${testcvs} update -r brnach1" \
		  "${PROG}"' \[[a-z]* aborted\]: no such tag brnach1'
		# Make sure we are still on the trunk
		if test -f file1 ; then
		    fail 92.1b
		else
		    pass 92.1b
		fi
		if test -f file3 ; then
		    pass 92.1c
		else
		    fail 92.1c
		fi

		# back to branch1
		if ${CVS} update -r branch1  2>> ${LOGFILE}; then
		    pass 93
		else
		    fail 93
		fi

		dotest_fail death-file4-6 "test -f file4" ''

		if test -f file1 ; then
		    pass 94
		else
		    fail 94
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'

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

	  # If the DIFF that CVS is using (options.h) is Sun diff, this
	  # test is said to fail (I think the /dev/null is the part
	  # that differs), along with a number of the other similar tests.
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'

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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
	  dotest death2-16 "${testcvs} -q commit -m add" \
"Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Add a new file on the branch.
	  echo "first revision" > file3
	  dotest death2-17 "${testcvs} add file3" \
"${PROG}"' [a-z]*: scheduling file `file3'\'' for addition on branch `branch'\''
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add these files permanently'
	  dotest branches-2a "${testcvs} -n -q ci -m dont-commit" ""
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
	  # The "cvs log file4" in test branches-14.3 will test that we
	  # didn't really add the tag.
	  dotest branches-3.3 "${testcvs} -qn tag dont-tag" \
"T file1
T file2
T file3
T file4"
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
date: [0-9/: ]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
trunk-change-after-branch
----------------------------
revision 1\.2
date: [0-9/: ]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
branches:  1\.2\.2;
trunk-before-branch
----------------------------
revision 1\.1
date: [0-9/: ]*;  author: ${username};  state: Exp;
add-it
----------------------------
revision 1\.2\.2\.2
date: [0-9/: ]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
change-on-br1
----------------------------
revision 1\.2\.2\.1
date: [0-9/: ]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
branches:  1\.2\.2\.1\.2;
modify
----------------------------
revision 1\.2\.2\.1\.2\.1
date: [0-9/: ]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
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

	  dotest branches-o1 "${testcvs} -q admin -o ::brbr" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file3,v
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file4,v
done"
	  cd ..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	rcslib)
	  # Test librarification of RCS.
	  # First: test whether `cvs diff' handles $Name expansion
	  # correctly.	We diff two revisions with their symbolic tags;
	  # neither tag should be expanded in the output.  Also diff
	  # one revision with the working copy.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest rcsdiff-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo "I am the first foo, and my name is $""Name$." > foo.c
	  dotest rcsdiff-2 "${testcvs} add -m new-file foo.c" \
"${PROG} [a-z]*: scheduling file .foo\.c. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest rcsdiff-3 "${testcvs} commit -m rev1 foo.c" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/foo\.c,v
done
Checking in foo\.c;
${TESTDIR}/cvsroot/first-dir/foo.c,v  <--  foo\.c
initial revision: 1\.1
done"
	  dotest rcsdiff-4 "${testcvs} tag first foo.c" "T foo\.c"
	  dotest rcsdiff-5 "${testcvs} update -p -r first foo.c" \
"===================================================================
Checking out foo\.c
RCS:  ${TESTDIR}/cvsroot/first-dir/foo\.c,v
VERS: 1\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
I am the first foo, and my name is \$""Name: first \$\."

	  echo "I am the second foo, and my name is $""Name$." > foo.c
	  dotest rcsdiff-6 "${testcvs} commit -m rev2 foo.c" \
"Checking in foo\.c;
${TESTDIR}/cvsroot/first-dir/foo\.c,v  <--  foo\.c
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest rcsdiff-7 "${testcvs} tag second foo.c" "T foo\.c"
	  dotest rcsdiff-8 "${testcvs} update -p -r second foo.c" \
"===================================================================
Checking out foo\.c
RCS:  ${TESTDIR}/cvsroot/first-dir/foo\.c,v
VERS: 1\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
I am the second foo, and my name is \$""Name: second \$\."

	dotest_fail rcsdiff-9 "${testcvs} diff -r first -r second" \
"${PROG} [a-z]*: Diffing \.
Index: foo\.c
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/foo\.c,v
retrieving revision 1\.1
retrieving revision 1\.2
diff -r1\.1 -r1\.2
1c1
< I am the first foo, and my name is \$""Name:  \$\.
---
> I am the second foo, and my name is \$""Name:  \$\."

	  echo "I am the once and future foo, and my name is $""Name$." > foo.c
	  dotest_fail rcsdiff-10 "${testcvs} diff -r first" \
"${PROG} [a-z]*: Diffing \.
Index: foo\.c
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/foo\.c,v
retrieving revision 1\.1
diff -r1\.1 foo\.c
1c1
< I am the first foo, and my name is \$""Name:  \$\.
---
> I am the once and future foo, and my name is \$""Name\$\."

	  # Test handling of libdiff options.  diff gets quite enough
	  # of a workout elsewhere in sanity.sh, so we assume that it's
	  # mostly working properly if it passes all the other tests.
	  # The main one we want to try is regex handling, since we are
	  # using CVS's regex matcher and not diff's.

	  cat >rgx.c <<EOF
test_regex (whiz, bang)
{
foo;
bar;
baz;
grumble;
}
EOF

	  dotest rcslib-diffrgx-1 "${testcvs} -q add -m '' rgx.c" \
"${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest rcslib-diffrgx-2 "${testcvs} -q ci -m '' rgx.c" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/rgx\.c,v
done
Checking in rgx\.c;
${TESTDIR}/cvsroot/first-dir/rgx\.c,v  <--  rgx\.c
initial revision: 1\.1
done"
	  cat >rgx.c <<EOF
test_regex (whiz, bang)
{
foo;
bar;
baz;
mumble;
}
EOF
	  # Use dotest_fail because exit status from `cvs diff' must be 1.
	  dotest_fail rcslib-diffrgx-3 "${testcvs} diff -c -F.*( rgx.c" \
"Index: rgx\.c
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/rgx\.c,v
retrieving revision 1\.1
diff -c -F\.\*( -r1\.1 rgx\.c
\*\*\* rgx\.c	[0-9/]* [0-9:]*	1\.1
--- rgx\.c	[0-9/]* [0-9:]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\* test_regex (whiz, bang)
\*\*\* 3,7 \*\*\*\*
  foo;
  bar;
  baz;
! grumble;
  }
--- 3,7 ----
  foo;
  bar;
  baz;
! mumble;
  }"

	  # Tests of rcsmerge/diff3.  Merge operations get a good general
	  # workout elsewhere; we want to make sure that options are still
	  # handled properly.  Try merging two branches with -kv, to test
	  # both -j and -k switches.

	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir

	  mkdir 1; cd 1
	  dotest rcslib-merge-1 "${testcvs} -q co -l ." ""
	  mkdir first-dir
	  dotest rcslib-merge-2 "${testcvs} -q add first-dir" \
"Directory ${TESTDIR}/cvsroot.*/first-dir added to the repository"
	  cd ..; rm -r 1

	  dotest rcslib-merge-3 "${testcvs} -q co first-dir" ""
	  cd first-dir

	  echo '$''Revision$' > file1
	  echo '2' >> file1
	  echo '3' >> file1
	  dotest rcslib-merge-4 "${testcvs} -q add file1" \
"${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest rcslib-merge-5 "${testcvs} -q commit -m '' file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  sed -e 's/2/two/' file1 > f; mv f file1
	  dotest rcslib-merge-6 "${testcvs} -q commit -m '' file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest rcslib-merge-7 "${testcvs} -q tag -b -r 1.1 patch1" "T file1"
	  dotest rcslib-merge-8 "${testcvs} -q update -r patch1" "[UP] file1"
	  dotest rcslib-merge-9 "${testcvs} -q status" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file1,v
   Sticky Tag:		patch1 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest rcslib-merge-10 "cat file1" \
'$''Revision: 1\.1 $
2
3'
	  sed -e 's/3/three/' file1 > f; mv f file1
	  dotest rcslib-merge-11 "${testcvs} -q commit -m '' file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest rcslib-merge-12 "${testcvs} -q update -kv -j1.2" \
"U file1
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into file1
rcsmerge: warning: conflicts during merge"
	  dotest rcslib-merge-13 "cat file1" \
"<<<<<<< file1
1\.1\.2\.1
2
three
[=]======
1\.2
two
3
[>]>>>>>> 1\.2"

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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
		# Tests of "cvs import":
		# basic2
		# rdiff  -- imports with keywords
		# import  -- more tests of imports with keywords
		# importb  -- -b option.
		# modules3
		# mflag -- various -m messages
		# ignore  -- import and cvsignore
		# binwrap -- import and -k wrappers
		# info -- imports which are rejected by verifymsg
		# head -- intended to test vendor branches and HEAD,
		#   although it doesn't really do it yet.

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
		    pass 96
		else
		    fail 96
		fi

		if cmp ../imported-f2-orig.tmp imported-f2; then
		  pass 96.5
		else
		  fail 96.5
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
		    pass 97
		else
		    fail 97
		fi

		cd first-dir
		for i in 1 2 3 4 ; do
			if test -f imported-f"$i" ; then
			    pass 98-$i
			else
			    fail 98-$i
			fi
		done
		if test -d RCS; then
		    fail 98.5
		else
		    pass 98.5
		fi

		# remove
		rm imported-f1
		if ${CVS} rm imported-f1  2>> ${LOGFILE}; then
		    pass 99
		else
		    fail 99
		fi

		# change
		echo local-change >> imported-f2

		# commit
		if ${CVS} ci -m local-changes  >> ${LOGFILE} 2>&1; then
		    pass 100
		else
		    fail 100
		fi

		# log
		if ${CVS} log imported-f1 | grep '1.1.1.2 (dead)'  ; then
		    fail 101
		else
		    pass 101
		fi

		# update into the vendor branch.
		if ${CVS} update -rvendor-branch  ; then
		    pass 102
		else
		    fail 102
		fi

		# remove file4 on the vendor branch
		rm imported-f4

		if ${CVS} rm imported-f4  2>> ${LOGFILE}; then
		    pass 103
		else
		    fail 103
		fi

		# commit
		if ${CVS} ci -m vendor-removed imported-f4 >>${LOGFILE}; then
		    pass 104
		else
		    fail 104
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
		    pass 105
		else
		    fail 105
		fi

		# second import - file4 deliberately unchanged
		cd ../import-dir
		for i in 1 2 3 ; do
			echo rev 2 of file $i >> imported-f"$i"
		done
		cp imported-f2 ../imported-f2-orig.tmp

		if ${CVS} import -m second-import first-dir vendor-branch junk-2_0  ; then
		    pass 106
		else
		    fail 106
		fi
		if cmp ../imported-f2-orig.tmp imported-f2; then
		  pass 106.5
		else
		  fail 106.5
		fi
		cd ..

		# co
		if ${CVS} co first-dir  ; then
		    pass 107
		else
		    fail 107
		fi

		cd first-dir

		if test -f imported-f1 ; then
		    fail 108
		else
		    pass 108
		fi

		for i in 2 3 ; do
			if test -f imported-f"$i" ; then
			    pass 109-$i
			else
			    fail 109-$i
			fi
		done

		# check vendor branch for file4
		if ${CVS} update -rvendor-branch  ; then
		    pass 110
		else
		    fail 110
		fi

		if test -f imported-f4 ; then
		    pass 111
		else
		    fail 111
		fi

		# update to main line
		if ${CVS} update -A  2>> ${LOGFILE}; then
		    pass 112
		else
		    fail 112
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
		    fail 114
		else
		    pass 114
		fi

		for i in 2 3 ; do
			if test -f imported-f"$i" ; then
			    pass 115-$i
			else
			    fail 115-$i
			fi
		done

		dotest import-116 'cat imported-f2' \
'imported file2
[<]<<<<<< imported-f2
import should not expand \$''Id: imported-f2,v 1\.2 [0-9/]* [0-9:]* '"${username}"' Exp \$
local-change
[=]======
import should not expand \$''Id: imported-f2,v 1\.1\.1\.2 [0-9/]* [0-9:]* '"${username}"' Exp \$
rev 2 of file 2
[>]>>>>>> 1\.1\.1\.2'

		cd ..
		rm -r first-dir
		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -r import-dir
		;;

	importb)
	  # More cvs import tests, especially -b option.

	  # OK, first we get some sources from the NetMunger project, and
	  # import them into the 1.1.1 vendor branch.
	  mkdir imp-dir
	  cd imp-dir
	  echo 'OpenMunger sources' >file1
	  echo 'OpenMunger sources' >file2
	  dotest importb-1 \
"${testcvs} import -m add first-dir openmunger openmunger-1_0" \
"N first-dir/file1
N first-dir/file2

No conflicts created by this import"
	  cd ..
	  rm -r imp-dir

	  # Now we put the sources we get from FreeMunger into 1.1.3
	  mkdir imp-dir
	  cd imp-dir
	  echo 'FreeMunger sources' >file1
	  echo 'FreeMunger sources' >file2
	  # Not completely sure how the conflict detection is supposed to
	  # be working here (haven't really thought about it).
	  dotest importb-2 \
"${testcvs} import -m add -b 1.1.3 first-dir freemunger freemunger-1_0" \
"C first-dir/file1
C first-dir/file2

2 conflicts created by this import.
Use the following command to help the merge:

	${PROG} checkout -jfreemunger:yesterday -jfreemunger first-dir"
	  cd ..
	  rm -r imp-dir

	  # Now a test of main branch import (into second-dir, not first-dir).
	  mkdir imp-dir
	  cd imp-dir
	  echo 'my own stuff' >mine1.c
	  echo 'my own stuff' >mine2.c
	  dotest_fail importb-3 \
"${testcvs} import -m add -b 1 second-dir dummy really_dumb_y" \
"${PROG} \[[a-z]* aborted\]: Only branches with two dots are supported: 1"
	  : when we implement main-branch import, should be \
"N second-dir/mine1\.c
N second-dir/mine2\.c

No conflicts created by this import"
	  cd ..
	  rm -r imp-dir

	  mkdir 1
	  cd 1
	  # when we implement main branch import, will want to 
	  # add "second-dir" here.
	  dotest importb-4 "${testcvs} -q co first-dir" \
"U first-dir/file1
U first-dir/file2"
	  cd first-dir
	  dotest importb-5 "${testcvs} -q log file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch: 1\.1\.1
locks: strict
access list:
symbolic names:
	freemunger-1_0: 1\.1\.3\.1
	freemunger: 1\.1\.3
	openmunger-1_0: 1\.1\.1\.1
	openmunger: 1\.1\.1
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.1;  1\.1\.3;
Initial revision
----------------------------
revision 1\.1\.3\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
add
----------------------------
revision 1\.1\.1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}0 -0
add
============================================================================="

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir ${CVSROOT_DIRNAME}/second-dir
	  ;;

	join)
	  # Test doing joins which involve adding and removing files.
	  #   Variety of scenarios (see list below), in the context of:
	  #     * merge changes from T1 to T2 into the main line
	  #     * merge changes from branch 'branch' into the main line
	  #     * merge changes from branch 'branch' into branch 'br2'.
	  # See also binfile2, which does similar things with binary files.
	  # See also join2, which tests joining (and update -A) on only
	  # a single file, rather than a directory.
	  # See also join3, which tests some cases involving the greatest
	  # common ancestor.  Here is a list of tests according to branch
	  # topology:
	  #
	  # --->bp---->trunk          too many to mention
	  #     \----->branch
	  #
	  #     /----->branch1
	  # --->bp---->trunk          multibranch
	  #     \----->branch2
	  #
	  # --->bp1----->bp2---->trunk   join3
	  #     \->br1   \->br2
	  #
	  # --->bp1----->trunk
	  #     \----bp2---->branch                branches
	  #          \------>branch-of-branch

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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add these files permanently'

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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add these files permanently'
	  dotest join-6 "${testcvs} rm file6 file8" \
"${PROG}"' [a-z]*: scheduling `file6'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file8'\'' for removal
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to remove these files permanently'
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add these files permanently'
	  dotest join-13 "${testcvs} rm file3 file4 file5 file6" \
"${PROG}"' [a-z]*: scheduling `file3'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file4'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file5'\'' for removal
'"${PROG}"' [a-z]*: scheduling `file6'\'' for removal
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to remove these files permanently'
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

	  cd ..

	  # Checkout the main line again and make a new branch which we
	  # merge to.
	  rm -r first-dir
	  dotest join-25 "${testcvs} -q co first-dir" \
'U first-dir/file2
U first-dir/file3
U first-dir/file4
U first-dir/file7'
	  cd first-dir
	  dotest join-26 "${testcvs} -q tag -b br2" \
"T file2
T file3
T file4
T file7"
	  dotest join-27 "${testcvs} -q update -r br2" ""
	  # The handling of file8 here looks fishy to me.  I don't see
	  # why it should be different from the case where we merge to
	  # the trunk (e.g. join-23).
	  dotest join-28 "${testcvs} -q update -j branch" \
"U file1
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
retrieving revision 1.1
retrieving revision 1.1.2.1
Merging differences between 1.1 and 1.1.2.1 into file2
${PROG} [a-z]*: scheduling file3 for removal
${PROG} [a-z]*: file file4 has been modified, but has been removed in revision branch
U file8"
	  # Verify that the right changes have been scheduled.
	  dotest join-29 "${testcvs} -q update" \
"A file1
M file2
R file3
A file8"

	  cd ../..

	  rm -r 1 2 3
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	join2)
	  # More joining tests.

	  # First the usual setup; create a directory first-dir, a file
	  # first-dir/file1, and a branch br1.
	  mkdir 1; cd 1
	  dotest join2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest join2-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
          cd first-dir
	  echo 'initial contents of file1' >file1
	  dotest join2-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest join2-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest join2-5 "${testcvs} -q tag -b br1" "T file1"
	  dotest join2-6 "${testcvs} -q update -r br1" ""
	  echo 'modify on branch' >>file1
	  touch bradd
	  dotest join2-6a "${testcvs} add bradd" \
"${PROG} [a-z]*: scheduling file .bradd. for addition on branch .br1.
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest join2-7 "${testcvs} -q ci -m modify" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/bradd,v
done
Checking in bradd;
${TESTDIR}/cvsroot/first-dir/Attic/bradd,v  <--  bradd
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Here is the unusual/pathological part.  We switch back to
	  # the trunk *for file1 only*, not for the whole directory.
	  dotest join2-8 "${testcvs} -q update -A file1" '[UP] file1'
	  dotest join2-9 "${testcvs} -q status file1" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest join2-10 "cat CVS/Tag" "Tbr1"

	  dotest join2-11 "${testcvs} -q update -j br1 file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file1"
	  dotest join2-12 "cat file1" "initial contents of file1
modify on branch"
	  # We should have no sticky tag on file1
	  dotest join2-13 "${testcvs} -q status file1" \
"===================================================================
File: file1            	Status: Locally Modified

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest join2-14 "cat CVS/Tag" "Tbr1"
	  # And the checkin should go to the trunk
	  dotest join2-15 "${testcvs} -q ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"

	  # OK, the above is all well and good and has worked for some
	  # time.  Now try the case where the file had been added on
	  # the branch.
	  dotest join2-16 "${testcvs} -q update -r br1" "[UP] file1"
	  # The workaround is to update the whole directory.
	  # The non-circumvented version won't work.  The reason is that
	  # update removes the entry from CVS/Entries, so of course we get
	  # the tag from CVS/Tag and not Entries.  I suppose maybe
	  # we could invent some new format in Entries which would handle
	  # this, but doing so, and handling it properly throughout
	  # CVS, would be a lot of work and I'm not sure this case justifies
	  # it.
	  dotest join2-17-circumvent "${testcvs} -q update -A" \
"${PROG} [a-z]*: warning: bradd is not (any longer) pertinent
[UP] file1"
:	  dotest join2-17 "${testcvs} -q update -A bradd" \
"${PROG} [a-z]*: warning: bradd is not (any longer) pertinent"
	  dotest join2-18 "${testcvs} -q update -j br1 bradd" "U bradd"
	  dotest join2-19 "${testcvs} -q status bradd" \
"===================================================================
File: bradd            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/Attic/bradd,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest join2-20 "${testcvs} -q ci -m modify bradd" \
"Checking in bradd;
${TESTDIR}/cvsroot/first-dir/bradd,v  <--  bradd
new revision: 1\.2; previous revision: 1\.1
done"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	join3)
	  # See "join" for a list of other joining/branching tests.
	  # First the usual setup; create a directory first-dir, a file
	  # first-dir/file1, and a branch br1.
	  mkdir 1; cd 1
	  dotest join3-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest join3-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  echo 'initial contents of file1' >file1
	  dotest join3-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest join3-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest join3-5 "${testcvs} -q tag -b br1" "T file1"
	  dotest join3-6 "${testcvs} -q update -r br1" ""
	  echo 'br1:line1' >>file1
	  dotest join3-7 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Now back to the trunk for:
	  # another revision and another branch for file1.
	  # add file2, which will exist on trunk and br2 but not br1.
	  dotest join3-8 "${testcvs} -q update -A" "[UP] file1"
	  echo 'trunk:line1' > file2
	  dotest join3-8a "${testcvs} add file2" \
"${PROG} [a-z]*: scheduling file .file2. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  echo 'trunk:line1' >>file1
	  dotest join3-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  dotest join3-10 "${testcvs} -q tag -b br2" "T file1
T file2"

	  # Before we actually have any revision on br2, let's try a join
	  dotest join3-11 "${testcvs} -q update -r br1" "[UP] file1
${PROG} [a-z]*: file2 is no longer in the repository"
	  dotest join3-12 "${testcvs} -q update -j br2" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into file1
rcsmerge: warning: conflicts during merge
U file2"
	  dotest join3-13 "cat file1" \
"initial contents of file1
[<]<<<<<< file1
br1:line1
[=]======
trunk:line1
[>]>>>>>> 1\.2"
	  rm file1

	  # OK, we'll try the same thing with a revision on br2.
	  dotest join3-14 "${testcvs} -q update -r br2 file1" \
"${PROG} [a-z]*: warning: file1 was lost
U file1" "U file1"
	  echo 'br2:line1' >>file1
	  dotest join3-15 "${testcvs} -q ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"

	  # OK, now we can join br2 to br1
	  dotest join3-16 "${testcvs} -q update -r br1 file1" "[UP] file1"
	  # It may seem odd, to merge a higher branch into a lower
	  # branch, but in fact CVS defines the ancestor as 1.1
	  # and so it merges both the 1.1->1.2 and 1.2->1.2.2.1 changes.
	  # This seems like a reasonably plausible behavior.
	  dotest join3-17 "${testcvs} -q update -j br2 file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.2\.2\.1
Merging differences between 1\.1 and 1\.2\.2\.1 into file1
rcsmerge: warning: conflicts during merge"
	  dotest join3-18 "cat file1" \
"initial contents of file1
[<]<<<<<< file1
br1:line1
[=]======
trunk:line1
br2:line1
[>]>>>>>> 1\.2\.2\.1"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	new) # look for stray "no longer pertinent" messages.
		mkdir ${CVSROOT_DIRNAME}/first-dir

		if ${CVS} co first-dir  ; then
		    pass 117
		else
		    fail 117
		fi

		cd first-dir
		touch a

		if ${CVS} add a  2>>${LOGFILE}; then
		    pass 118
		else
		    fail 118
		fi

		if ${CVS} ci -m added  >>${LOGFILE} 2>&1; then
		    pass 119
		else
		    fail 119
		fi

		rm a

		if ${CVS} rm a  2>>${LOGFILE}; then
		    pass 120
		else
		    fail 120
		fi

		if ${CVS} ci -m removed >>${LOGFILE} ; then
		    pass 121
		else
		    fail 121
		fi

		if ${CVS} update -A  2>&1 | grep longer ; then
		    fail 122
		else
		    pass 122
		fi

		if ${CVS} update -rHEAD 2>&1 | grep longer ; then
		    fail 123
		else
		    pass 123
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
File: a                	Status: Needs Patch

   Working revision:	1\.1.*
   Repository revision:	1\.1\.2\.1	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		branch (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)" \
"===================================================================
File: a                	Status: Needs Checkout

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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
		    pass 127
		else
		    fail 127
		fi
		cd first-dir
		if test -f a; then
		    pass 127a
		else
		    fail 127a
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
		dotest conflicts-132 "${testcvs} -q ci -m try" \
"${PROG} [a-z]*: warning: file .a. seems to still contain conflict indicators
Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
new revision: 1\.3; previous revision: 1\.2
done"

		# OK, the user saw the warning (good user), and now
		# resolves it for real.
		echo resolve conflict >a
		dotest conflicts-status-3 "${testcvs} status a" \
"===================================================================
File: a                	Status: Locally Modified

   Working revision:	1\.3.*
   Repository revision:	1\.3	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest conflicts-133 "${testcvs} -q ci -m resolved" \
"Checking in a;
${TESTDIR}/cvsroot/first-dir/a,v  <--  a
new revision: 1\.4; previous revision: 1\.3
done"
		dotest conflicts-status-4 "${testcvs} status a" \
"===================================================================
File: a                	Status: Up-to-date

   Working revision:	1\.4.*
   Repository revision:	1\.4	${TESTDIR}/cvsroot/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

		# Now test that we can add a file in one working directory
		# and have an update in another get it.
		cd ../../1/first-dir
		echo abc >abc
		if ${testcvs} add abc >>${LOGFILE} 2>&1; then
		    pass 134
		else
		    fail 134
		fi
		if ${testcvs} ci -m 'add abc' abc >>${LOGFILE} 2>&1; then
		    pass 135
		else
		    fail 135
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
		    pass 138
		else
		    fail 138
		fi
		cd ../..
		mkdir 3
		cd 3
		if ${testcvs} -q co first-dir/abc first-dir/subdir \
		    >>${LOGFILE}; then
		    pass 139
		else
		    fail 139
		fi
		cd ../1/first-dir/subdir
		echo sss >sss
		if ${testcvs} add sss >>${LOGFILE} 2>&1; then
		    pass 140
		else
		    fail 140
		fi
		if ${testcvs} ci -m adding sss >>${LOGFILE} 2>&1; then
		    pass 140
		else
		    fail 140
		fi
		cd ../../../3/first-dir
		if ${testcvs} -q update >>${LOGFILE}; then
		    pass 141
		else
		    fail 141
		fi
		if test -f subdir/sss; then
		    pass 142
		else
		    fail 142
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
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
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
	  echo 'contents unchanged' >same.c
	  dotest conflicts2-142d0 "${testcvs} add aa.c same.c" \
"${PROG} [a-z]*: scheduling file .aa\.c. for addition
${PROG} [a-z]*: scheduling file .same\.c. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest conflicts2-142d1 "${testcvs} -q ci -m added" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aa\.c,v
done
Checking in aa\.c;
${TESTDIR}/cvsroot/first-dir/aa\.c,v  <--  aa\.c
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/same\.c,v
done
Checking in same\.c;
${TESTDIR}/cvsroot/first-dir/same\.c,v  <--  same\.c
initial revision: 1\.1
done"
	  cd ../../2/first-dir
	  echo "don't you dare obliterate this text" >aa.c
	  echo 'contents unchanged' >same.c
	  # Note the discrepancy between local and remote in the handling
	  # of same.c.  I kind
	  # of suspect that the local CVS behavior is the more useful one
	  # although I do sort of wonder whether we should make people run
	  # cvs add just to get them in that habit (also, trying to implement
	  # the local CVS behavior for remote without the cvs add seems 
	  # pretty difficult).
	  if test "$remote" = yes; then
	    dotest_fail conflicts2-142d2 "${testcvs} -q update" \
"${QUESTION} aa\.c
${QUESTION} same\.c
${PROG} update: move away \./aa\.c; it is in the way
C aa\.c
${PROG} update: move away \./same\.c; it is in the way
C same\.c"
	  else
	    dotest_fail conflicts2-142d2 "${testcvs} -q update" \
"${PROG} [a-z]*: move away aa\.c; it is in the way
C aa\.c
U same\.c"
	  fi
	  dotest conflicts2-142d3 "${testcvs} -q status aa.c" \
"${PROG} [a-z]*: move away aa\.c; it is in the way
===================================================================
File: aa\.c             	Status: Unresolved Conflict

   Working revision:	No entry for aa\.c
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/aa\.c,v"

	  # Could also be testing the case in which the cvs add happened
	  # before the commit by the other user.
	  # This message seems somewhat bogus.  I mean, parallel development
	  # means that we get to work in parallel if we choose, right?  And
	  # then at commit time it would be a conflict.
	  dotest_fail conflicts2-142d4 "${testcvs} -q add aa.c" \
"${PROG} [a-z]*: aa.c added independently by second party"

	  # The user might want to see just what the conflict is.
	  # Don't bother, diff seems to kind of lose its mind, with or
	  # without -N.  This is a CVS bug(s).
	  #dotest conflicts2-142d5 "${testcvs} -q diff -r HEAD -N aa.c" fixme

	  # Now: "how can the user resolve this conflict", I hear you cry.
	  # Well, one way is to forget about the file in the working
	  # directory.
	  # Since it didn't let us do the add in conflicts2-142d4, there
	  # is no need to run cvs rm here.
	  #dotest conflicts2-142d6 "${testcvs} -q rm -f aa.c" fixme
	  dotest conflicts2-142d6 "rm aa.c" ''
	  dotest conflicts2-142d7 "${testcvs} -q update aa.c" "U aa\.c"
	  dotest conflicts2-142d8 "cat aa.c" ''

	  # The other way is to use the version from the working directory
	  # instead of the version from the repository.  Unfortunately,
	  # there doesn't seem to be any particularly clear way to do
	  # this (?).

	  cd ../..

	  rm -r 1 2 ; rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	conflicts3)
	  # More tests of conflicts and/or multiple working directories
	  # in general.

	  mkdir 1; cd 1
	  dotest conflicts3-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest conflicts3-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd ..
	  mkdir 2; cd 2
	  dotest conflicts3-3 "${testcvs} -q co -l first-dir" ''
	  cd ../1/first-dir
	  touch file1 file2
	  dotest conflicts3-4 "${testcvs} add file1 file2" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: scheduling file .file2. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest conflicts3-5 "${testcvs} -q ci -m add-them" \
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
	  cd ../../2/first-dir
	  # Check that -n doesn't make CVS lose its mind as it creates
	  # (or rather, doesn't) a new file.
	  dotest conflicts3-6 "${testcvs} -nq update" \
"U file1
U file2"
	  dotest_fail conflicts3-7 "test -f file1" ''
	  dotest conflicts3-8 "${testcvs} -q update" \
"U file1
U file2"
	  dotest conflicts3-9 "test -f file2" ''

	  # OK, now remove two files at once
	  dotest conflicts3-10 "${testcvs} rm -f file1 file2" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: scheduling .file2. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove these files permanently"
	  dotest conflicts3-11 "${testcvs} -q ci -m remove-them" \
"Removing file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done
Removing file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done"
	  cd ../../1/first-dir
	  dotest conflicts3-12 "${testcvs} -n -q update" \
"${PROG} [a-z]*: warning: file1 is not (any longer) pertinent
${PROG} [a-z]*: warning: file2 is not (any longer) pertinent"
	  dotest conflicts3-13 "${testcvs} -q update" \
"${PROG} [a-z]*: warning: file1 is not (any longer) pertinent
${PROG} [a-z]*: warning: file2 is not (any longer) pertinent"

	  # OK, now add a directory to both working directories
	  # and see that CVS doesn't lose its mind.
	  mkdir sdir
	  dotest conficts3-14 "${testcvs} add sdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir added to the repository"
	  cd ../../2/first-dir

	  # Create a CVS directory without the proper administrative
	  # files in it.  This can happen for example if you hit ^C
	  # in the middle of a checkout.
	  mkdir sdir
	  mkdir sdir/CVS
	  # OK, in the local case CVS sees that the directory exists
	  # in the repository and recurses into it.  In the remote case
	  # CVS can't see the repository and has no way of knowing
	  # that sdir is even a directory (stat'ing everything would be
	  # too slow).  The remote behavior makes more sense to me (but
	  # would this affect other cases?).
	  if test "$remote" = yes; then
	    dotest conflicts3-15 "${testcvs} -q update" \
"${QUESTION} sdir"
	  else
	    dotest conflicts3-15 "${testcvs} -q update" \
"${QUESTION} sdir
${PROG} [a-z]*: ignoring sdir (CVS/Repository missing)"
	    touch sdir/CVS/Repository
	    dotest conflicts3-16 "${testcvs} -q update" \
"${QUESTION} sdir
${PROG} [a-z]*: ignoring sdir (CVS/Entries missing)"
	  fi
	  rm -r sdir

	  # OK, now the same thing, but the directory doesn't exist
	  # in the repository.
	  mkdir newdir
	  mkdir newdir/CVS
	  dotest conflicts3-17 "${testcvs} -q update" "${QUESTION} newdir"
	  echo "D/newdir////" >> CVS/Entries
	  dotest conflicts3-18 "${testcvs} -q update" \
"${PROG} [a-z]*: ignoring newdir (CVS/Repository missing)"
	  touch newdir/CVS/Repository
	  dotest conflicts3-19 "${testcvs} -q update" \
"${PROG} [a-z]*: ignoring newdir (CVS/Entries missing)"
	  rm -r newdir

	  cd ../..

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules)
	  # Tests of various ways to define and use modules.

	  ############################################################
	  # These tests are to make sure that administrative files get
	  # rebuilt, regardless of how and where files are checked
	  # out.
	  ############################################################
	  # Check out the whole repository
	  mkdir 1; cd 1
	  dotest modules-1 "${testcvs} -q co ." 'U CVSROOT/checkoutlist
U CVSROOT/commitinfo
U CVSROOT/config
U CVSROOT/cvswrappers
U CVSROOT/editinfo
U CVSROOT/loginfo
U CVSROOT/modules
U CVSROOT/notify
U CVSROOT/rcsinfo
U CVSROOT/taginfo
U CVSROOT/verifymsg'
	  echo "# made a change" >>CVSROOT/modules
	  dotest modules-1d "${testcvs} -q ci -m add-modules" \
"Checking in CVSROOT/modules;
${TESTDIR}/cvsroot/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..
	  rm -rf 1

	  ############################################################
	  # Check out CVSROOT
	  mkdir 1; cd 1
	  dotest modules-2 "${testcvs} -q co CVSROOT" 'U CVSROOT/checkoutlist
U CVSROOT/commitinfo
U CVSROOT/config
U CVSROOT/cvswrappers
U CVSROOT/editinfo
U CVSROOT/loginfo
U CVSROOT/modules
U CVSROOT/notify
U CVSROOT/rcsinfo
U CVSROOT/taginfo
U CVSROOT/verifymsg'
	  echo "# made a change" >>CVSROOT/modules
	  dotest modules-2d "${testcvs} -q ci -m add-modules" \
"Checking in CVSROOT/modules;
${TESTDIR}/cvsroot/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..
	  rm -rf 1

	  ############################################################
	  # Check out CVSROOT in some other directory
	  mkdir ${CVSROOT_DIRNAME}/somedir
	  mkdir 1; cd 1
	  dotest modules-3 "${testcvs} -q co somedir" ''
	  cd somedir
	  dotest modules-3d "${testcvs} -q co CVSROOT" 'U CVSROOT/checkoutlist
U CVSROOT/commitinfo
U CVSROOT/config
U CVSROOT/cvswrappers
U CVSROOT/editinfo
U CVSROOT/loginfo
U CVSROOT/modules
U CVSROOT/notify
U CVSROOT/rcsinfo
U CVSROOT/taginfo
U CVSROOT/verifymsg'
	  echo "# made a change" >>CVSROOT/modules
	  dotest modules-3g "${testcvs} -q ci -m add-modules" \
"Checking in CVSROOT/modules;
${TESTDIR}/cvsroot/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/somedir
	  ############################################################
	  # end rebuild tests
	  ############################################################


	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  mkdir 1
	  cd 1

	  if ${testcvs} -q co first-dir; then
	      pass 143
	  else
	      fail 143
	  fi

	  cd first-dir
	  mkdir subdir
	  ${testcvs} add subdir >>${LOGFILE}
	  cd subdir

	  mkdir ssdir
	  ${testcvs} add ssdir >>${LOGFILE}

	  touch a b

	  if ${testcvs} add a b 2>>${LOGFILE} ; then
	      pass 144
	  else
	      fail 144
	  fi

	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	      pass 145
	  else
	      fail 145
	  fi

	  cd ..
	  if ${testcvs} -q co CVSROOT >>${LOGFILE}; then
	      pass 146
	  else
	      fail 146
	  fi

	  # Here we test that CVS can deal with CVSROOT (whose repository
	  # is at top level) in the same directory as subdir (whose repository
	  # is a subdirectory of first-dir).  TODO: Might want to check that
	  # files can actually get updated in this state.
	  if ${testcvs} -q update; then
	      pass 147
	  else
	      fail 147
	  fi

	  echo realmodule first-dir/subdir a >CVSROOT/modules
	  echo dirmodule first-dir/subdir >>CVSROOT/modules
	  echo namedmodule -d nameddir first-dir/subdir >>CVSROOT/modules
	  echo aliasmodule -a first-dir/subdir/a >>CVSROOT/modules
	  echo aliasnested -a first-dir/subdir/ssdir >>CVSROOT/modules
	  echo topfiles -a first-dir/file1 first-dir/file2 >>CVSROOT/modules
	  echo world -a . >>CVSROOT/modules
	  echo statusmod -s Mungeable >>CVSROOT/modules

	  # Options must come before arguments.  It is possible this should
	  # be relaxed at some point (though the result would be bizarre for
	  # -a); for now test the current behavior.
	  echo bogusalias first-dir/subdir/a -a >>CVSROOT/modules
	  if ${testcvs} ci -m 'add modules' CVSROOT/modules \
	      >>${LOGFILE} 2>&1; then
	      pass 148
	  else
	      fail 148
	  fi
	  cd ..
	  # The "statusmod" module contains an error; trying to use it
	  # will produce "modules file missing directory" I think.
	  # However, that shouldn't affect the ability of "cvs co -c" or
	  # "cvs co -s" to do something reasonable with it.
	  dotest 148a0 "${testcvs} co -c" 'aliasmodule  -a first-dir/subdir/a
aliasnested  -a first-dir/subdir/ssdir
bogusalias   first-dir/subdir/a -a
dirmodule    first-dir/subdir
namedmodule  -d nameddir first-dir/subdir
realmodule   first-dir/subdir a
statusmod    -s Mungeable
topfiles     -a first-dir/file1 first-dir/file2
world        -a \.'
	  # There is code in modules.c:save_d which explicitly skips
	  # modules defined with -a, which is why aliasmodule is not
	  # listed.
	  dotest 148a1 "${testcvs} co -s" \
'statusmod    Mungeable  
bogusalias   NONE        first-dir/subdir/a -a
dirmodule    NONE        first-dir/subdir
namedmodule  NONE        first-dir/subdir
realmodule   NONE        first-dir/subdir a'

	  # Test that real modules check out to realmodule/a, not subdir/a.
	  if ${testcvs} co realmodule >>${LOGFILE}; then
	      pass 149a1
	  else
	      fail 149a1
	  fi
	  if test -d realmodule && test -f realmodule/a; then
	      pass 149a2
	  else
	      fail 149a2
	  fi
	  if test -f realmodule/b; then
	      fail 149a3
	  else
	      pass 149a3
	  fi
	  if ${testcvs} -q co realmodule; then
	      pass 149a4
	  else
	      fail 149a4
	  fi
	  if echo "yes" | ${testcvs} release -d realmodule >>${LOGFILE} ; then
	      pass 149a5
	  else
	      fail 149a5
	  fi

	  dotest_fail 149b1 "${testcvs} co realmodule/a" \
"${PROG}"' [a-z]*: module `realmodule/a'\'' is a request for a file in a module which is not a directory' \
"${PROG}"' [a-z]*: module `realmodule/a'\'' is a request for a file in a module which is not a directory
'"${PROG}"' \[[a-z]* aborted\]: cannot expand modules'

	  # Now test the ability to check out a single file from a directory
	  if ${testcvs} co dirmodule/a >>${LOGFILE}; then
	      pass 150c
	  else
	      fail 150c
	  fi
	  if test -d dirmodule && test -f dirmodule/a; then
	      pass 150d
	  else
	      fail 150d
	  fi
	  if test -f dirmodule/b; then
	      fail 150e
	  else
	      pass 150e
	  fi
	  if echo "yes" | ${testcvs} release -d dirmodule >>${LOGFILE} ; then
	      pass 150f
	  else
	      fail 150f
	  fi
	  # Now test the ability to correctly reject a non-existent filename.
	  # For maximum studliness we would check that an error message is
	  # being output.
	  if ${testcvs} co dirmodule/nonexist >>${LOGFILE} 2>&1; then
	    # We accept a zero exit status because it is what CVS does
	    # (Dec 95).  Probably the exit status should be nonzero,
	    # however.
	      pass 150g1
	  else
	      pass 150g1
	  fi
	  # We tolerate the creation of the dirmodule directory, since that
	  # is what CVS does, not because we view that as preferable to not
	  # creating it.
	  if test -f dirmodule/a || test -f dirmodule/b; then
	      fail 150g2
	  else
	      pass 150g2
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
	      pass 151
	  else
	      fail 151
	  fi
	  if test -d aliasmodule; then
	      fail 152
	  else
	      pass 152
	  fi
	  echo abc >>first-dir/subdir/a
	  if (${testcvs} -q co aliasmodule | tee test153.tmp) \
	      >>${LOGFILE}; then
	      pass 153
	  else
	      fail 153
	  fi
	  echo 'M first-dir/subdir/a' >ans153.tmp
	  if cmp test153.tmp ans153.tmp; then
	      pass 154
	  else
	      fail 154
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add these files permanently'
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

	  # Make sure the right thing happens if we remove a file.
	  cd first-dir
	  dotest modules-155c6 "${testcvs} -q rm -f file1" \
"${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  dotest modules-155c7 "${testcvs} -q ci -m remove-it" \
"Removing file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  cd ..
	  rm -r first-dir
	  dotest modules-155c8 "${testcvs} -q co topfiles" \
"${PROG} [a-z]*: warning: first-dir/file1 is not (any longer) pertinent
U first-dir/file2"

	  cd ..
	  rm -r 1

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules2)
	  # More tests of modules, in particular the & feature.
	  mkdir 1; cd 1
	  dotest modules2-setup-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir second-dir third-dir
	  dotest modules2-setup-2 \
"${testcvs} add first-dir second-dir third-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository
Directory ${TESTDIR}/cvsroot/second-dir added to the repository
Directory ${TESTDIR}/cvsroot/third-dir added to the repository"
	  cd third-dir
	  touch file3
	  dotest modules2-setup-3 "${testcvs} add file3" \
"${PROG} [a-z]*: scheduling file .file3. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest modules2-setup-4 "${testcvs} -q ci -m add file3" \
"RCS file: ${TESTDIR}/cvsroot/third-dir/file3,v
done
Checking in file3;
${TESTDIR}/cvsroot/third-dir/file3,v  <--  file3
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  mkdir 1
	  cd 1

	  dotest modules2-1 "${testcvs} -q co CVSROOT/modules" \
'U CVSROOT/modules'
	  cd CVSROOT
	  echo 'ampermodule &first-dir &second-dir' > modules
	  echo 'combmodule third-dir file3 &first-dir' >> modules
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

	  # Now we create another directory named first-dir and make
	  # sure that CVS doesn't get them mixed up.
	  mkdir first-dir
	  # Note that this message should say "Updating ampermodule/first-dir"
	  # I suspect.  This is a long-standing behavior/bug....
	  dotest modules2-9 "${testcvs} co ampermodule" \
"${PROG} [a-z]*: Updating first-dir
${PROG} [a-z]*: Updating second-dir"
	  touch ampermodule/first-dir/amper1
	  dotest modules2-10 "${testcvs} add ampermodule/first-dir/amper1" \
"${PROG} [a-z]*: scheduling file .ampermodule/first-dir/amper1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"

	  # As with the "Updating xxx" message, the "U first-dir/amper1"
	  # message (instead of "U ampermodule/first-dir/amper1") is
	  # rather fishy.
	  dotest modules2-12 "${testcvs} co ampermodule" \
"${PROG} [a-z]*: Updating first-dir
A first-dir/amper1
${PROG} [a-z]*: Updating second-dir"

	  if test "$remote" = no; then
	    dotest modules2-13 "${testcvs} -q ci -m add-it ampermodule" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/amper1,v
done
Checking in ampermodule/first-dir/amper1;
${TESTDIR}/cvsroot/first-dir/amper1,v  <--  amper1
initial revision: 1\.1
done"
	  else
	    # Trying this as above led to a "protocol error" message.
	    # Work around this bug.
	    cd ampermodule
	    dotest modules2-13 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/amper1,v
done
Checking in first-dir/amper1;
${TESTDIR}/cvsroot/first-dir/amper1,v  <--  amper1
initial revision: 1\.1
done"
	    cd ..
	  fi

	  # Now test the "combmodule" module (combining regular modules
	  # and ampersand modules in the same module definition).
	  cd ..
	  rm -r 1
	  mkdir 1; cd 1
	  dotest modules2-14 "${testcvs} co combmodule" \
"U combmodule/file3
${PROG} [a-z]*: Updating first-dir
U first-dir/amper1"
	  dotest modules2-15 "test -f combmodule/file3" ""
	  dotest modules2-16 "test -f combmodule/first-dir/amper1" ""
	  cd combmodule
	  rm -r first-dir
	  # Might be possible to have a more graceful error message,
	  # but at least for now there is no way to tell CVS that
	  # some files/subdirectories come from one repository directory,
	  # and others from another.
	  if test "$remote" = no; then
	    dotest_fail modules2-17 "${testcvs} update -d" \
"${PROG} [a-z]*: Updating \.
${PROG} [a-z]*: Updating first-dir
${PROG} \[[a-z]* aborted\]: cannot open directory ${TESTDIR}/cvsroot/third-dir/first-dir: No such file or directory"
	    # Clean up the droppings left by the previous command.
	    # This should definitely not be necessary (I think).
	    rm -r first-dir
	  else
	    # This seems like a pretty sensible behavior to me, in the
	    # sense that first-dir doesn't "really" exist within
	    # third-dir, so CVS just acts as if there is nothing there
	    # to do.
	    dotest modules2-17 "${testcvs} update -d" \
"${PROG} server: Updating \."
	  fi

	  cd ..
	  dotest modules2-18 "${testcvs} -q co combmodule" \
"U first-dir/amper1"
	  dotest modules2-19 "test -f combmodule/first-dir/amper1" ""
	  cd ..
	  rm -r 1

	  # Test that CVS gives an error if one combines -a with
	  # other options.
	  # Probably would be better to break this out into a separate
	  # test.  Although it is short, it shares no files/state with
	  # the rest of the modules2 tests.
	  mkdir 1; cd 1
	  dotest modules2-a0.5 "${testcvs} -q co CVSROOT/modules" \
'U CVSROOT/modules'
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
	  rm -rf ${CVSROOT_DIRNAME}/third-dir
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
	  dotest modules3-3 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..

	  dotest modules3-4 "${testcvs} -q update -d CVSROOT" \
"U CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  cat >modules <<EOF
mod1 -a first-dir/file1
bigmod -a mod1 first-dir/file1
namednest -d src/sub/dir first-dir
nestdeeper -d src/sub1/sub2/sub3/dir first-dir
nestshallow -d src/dir second-dir/suba/subb
path/in/modules &mod1
another/path/test -d another/path/test first-dir
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest modules3-7d "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/second-dir/suba/subb/fileb,v
done
Checking in fileb;
${TESTDIR}/cvsroot/second-dir/suba/subb/fileb,v  <--  fileb
initial revision: 1\.1
done"
	  cd ../../..
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
	  # "internal error: repository string too short." (CVS 1.9) or
	  # "warning: server is not creating directories one at a time" (now)
	  # instead of a real error).
	  # I'm tempted to just make it a fatal error to have '/' in a
	  # module name.  But see comments at modules3-16.
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

	  # Some people seem to want this to work.  I still suspect there
	  # are dark corners in slashes in module names.  This probably wants
	  # more thought before we start hacking on CVS (one way or the other)
	  # or documenting this.
	  mkdir 2; cd 2
	  dotest modules3-16 "${testcvs} -q co another/path/test" \
"U another/path/test/file1"
	  dotest modules3-17 "cat another/path/test/file1" 'file1'
	  cd ..; rm -r 2

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
	        pass 156
	    else
		fail 156
	    fi
	    # Must import twice since the first time uses inline code that
	    # avoids RCS call.
	    echo testb >>test
	    if ${testcvs} import -m "$message" a-dir A A2 >>${LOGFILE} 2>&1;then
		pass 157
	    else
		fail 157
	    fi
	    # Test handling of -m during ci
	    cd ..; rm -r a-dir
	    if ${testcvs} co a-dir >>${LOGFILE} 2>&1; then
		pass 158
	    else
		fail 158
	    fi
	    cd a-dir
	    echo testc >>test
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
		pass 159
	    else
		fail 159
	    fi
	    # Test handling of -m during rm/ci
	    rm test;
	    if ${testcvs} rm test >>${LOGFILE} 2>&1; then
		pass 160
	    else
		fail 160
	    fi
	    if ${testcvs} ci -m "$message" >>${LOGFILE} 2>&1; then
		pass 161
	    else
		fail 161
	    fi
	    # Clean up
	    cd ..
	    rm -r a-dir
	    rm -rf ${CVSROOT_DIRNAME}/a-dir
	  done
	  ;;

	editor)
	  # More tests of log messages, in this case the ability to
	  # run an external editor.
	  # TODO:
	  #   * also test $EDITOR, $CVSEDITOR, &c.
	  #   * test what happens if up-to-date check fails.

	  # Our "editor" puts "x" at the start of each line, so we
	  # can see the "CVS:" lines.
	  cat >${TESTDIR}/editme <<EOF
#!${TESTSHELL}
sleep 1
sed <\$1 -e 's/^/x&/g' >${TESTDIR}/edit.new
mv ${TESTDIR}/edit.new \$1
exit 0
EOF
	  chmod +x ${TESTDIR}/editme

	  mkdir 1; cd 1
	  dotest editor-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest editor-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  touch file1 file2
	  dotest editor-3 "${testcvs} add file1 file2" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: scheduling file .file2. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest editor-4 "${testcvs} -e ${TESTDIR}/editme -q ci" \
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
	  dotest editor-5 "${testcvs} -q tag -b br" "T file1
T file2"
	  dotest editor-6 "${testcvs} -q update -r br" ''
	  echo modify >>file1
	  dotest editor-7 "${testcvs} -e ${TESTDIR}/editme -q ci" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  # OK, now we want to make sure "ci -r" puts in the branch
	  # where appropriate.  Note that we can check in on the branch
	  # without being on the branch, because there is not a revision
	  # already on the branch.  If there were a revision on the branch,
	  # CVS would correctly give an up-to-date check failed.
	  dotest editor-8 "${testcvs} -q update -A" "U file1"
	  echo add a line >>file2
	  dotest editor-9 "${testcvs} -q -e ${TESTDIR}/editme ci -rbr file2" \
"Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  dotest editor-log-file1 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;
x
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Committing in .
xCVS:
xCVS: Added Files:
xCVS: 	file1 file2
xCVS: ----------------------------------------------------------------------
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
x
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Committing in .
xCVS:
xCVS: Modified Files:
xCVS:  Tag: br
xCVS: 	file1
xCVS: ----------------------------------------------------------------------
============================================================================="

	  # The only difference between the two expect strings is the
	  # presence or absence of "Committing in ." for 1.1.2.1.
	  dotest editor-log-file2 "${testcvs} log -N file2" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
Working file: file2
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;
x
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Committing in .
xCVS:
xCVS: Added Files:
xCVS: 	file1 file2
xCVS: ----------------------------------------------------------------------
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
x
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Modified Files:
xCVS:  Tag: br
xCVS: 	file2
xCVS: ----------------------------------------------------------------------
=============================================================================" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
Working file: file2
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;
x
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Committing in .
xCVS:
xCVS: Added Files:
xCVS: 	file1 file2
xCVS: ----------------------------------------------------------------------
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
x
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Committing in .
xCVS:
xCVS: Modified Files:
xCVS:  Tag: br
xCVS: 	file2
xCVS: ----------------------------------------------------------------------
============================================================================="
	  cd ../..
	  rm -r 1
	  rm ${TESTDIR}/editme
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	errmsg1)
	  mkdir ${CVSROOT_DIRNAME}/1dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co 1dir; then
	      pass 162
	  else
	      fail 162
	  fi
	  cd 1dir
	  touch foo
	  if ${testcvs} add foo 2>>${LOGFILE}; then
	      pass 163
	  else
	      fail 163
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	      pass 164
	  else
	      fail 164
	  fi
	  cd ../..
	  mkdir 2
	  cd 2
	  if ${testcvs} -q co 1dir >>${LOGFILE}; then
	      pass 165
	  else
	      fail 165
	  fi
	  chmod a-w 1dir
	  cd ../1/1dir
	  rm foo;
	  if ${testcvs} rm foo >>${LOGFILE} 2>&1; then
	      pass 166
	  else
	      fail 166
	  fi
	  if ${testcvs} ci -m removed >>${LOGFILE} 2>&1; then
	      pass 167
	  else
	      fail 167
	  fi

	  cd ../../2/1dir
	  # FIXME: should be using dotest.
	  ${testcvs} -q update 2>../tst167.err
	  cat ../tst167.err >>${LOGFILE}
	  cat <<EOF >../tst167.ans
${PROG} server: warning: foo is not (any longer) pertinent
${PROG} update: unable to remove ./foo: Permission denied
EOF
	  if cmp ../tst167.ans ../tst167.err >/dev/null ||
	  ( echo "${PROG} [update aborted]: cannot rename file foo to CVS/,,foo: Permission denied" | cmp - ../tst167.err >/dev/null )
	  then
	      pass 168
	  else
	      fail 168
	  fi

	  cd ..
	  chmod u+w 1dir
	  cd ..
	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/1dir
	  ;;

	errmsg2)
	  # More tests of various miscellaneous error handling,
	  # and cvs add behavior in general.
	  # See also test basicb-4a, concerning "cvs ci CVS".
	  # Too many tests to mention test the simple cases of
	  # adding files and directories.
	  # Test basicb-2a10 tests cvs -n add.

	  # First the usual setup; create a directory first-dir.
	  mkdir 1; cd 1
	  dotest errmsg2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest errmsg2-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
          cd first-dir
	  dotest_fail errmsg2-3 "${testcvs} add CVS" \
"${PROG} [a-z]*: cannot add special file .CVS.; skipping"
	  touch file1
	  # For the most part add returns a failure exitstatus if
	  # there are any errors, even if the remaining files are
	  # processed without incident.  The "cannot add
	  # special file" message fits this pattern, at
	  # least currently.
	  dotest_fail errmsg2-4 "${testcvs} add CVS file1" \
"${PROG} [a-z]*: cannot add special file .CVS.; skipping
${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  # I'm not sure these tests completely convey the various strange
	  # behaviors that CVS had before it specially checked for "." and
	  # "..".  Suffice it to say that these are unlikely to work right
	  # without a special case.
	  dotest_fail errmsg2-5 "${testcvs} add ." \
"${PROG} [a-z]*: cannot add special file .\..; skipping"
	  dotest_fail errmsg2-6 "${testcvs} add .." \
"${PROG} [a-z]*: cannot add special file .\.\..; skipping"
	  # Make sure that none of the error messages left droppings
	  # which interfere with normal operation.
	  dotest errmsg2-7 "${testcvs} -q ci -m add-file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  mkdir sdir
	  cd ..
	  dotest errmsg2-8 "${testcvs} add first-dir/sdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir added to the repository"

	  cd first-dir

	  touch file10
	  mkdir sdir10
	  dotest errmsg2-10 "${testcvs} add file10 sdir10" \
"${PROG} [a-z]*: scheduling file .file10. for addition
Directory ${TESTDIR}/cvsroot/first-dir/sdir10 added to the repository
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest errmsg2-11 "${testcvs} -q ci -m add-file10" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file10,v
done
Checking in file10;
${TESTDIR}/cvsroot/first-dir/file10,v  <--  file10
initial revision: 1\.1
done"
	  # Try to see that there are no droppings left by
	  # any of the previous tests.
	  dotest errmsg2-12 "${testcvs} -q update" ""

	  # Now test adding files with '/' in the name, both one level
	  # down and more than one level down.
	  cd ..
	  mkdir first-dir/sdir10/ssdir
	  dotest errmsg2-13 "${testcvs} add first-dir/sdir10/ssdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/sdir10/ssdir added to the repository"

	  touch first-dir/sdir10/ssdir/ssfile
	  dotest errmsg2-14 \
	    "${testcvs} add first-dir/sdir10/ssdir/ssfile" \
"${PROG} [a-z]*: scheduling file .first-dir/sdir10/ssdir/ssfile. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  touch first-dir/file15
	  dotest errmsg2-15 "${testcvs} add first-dir/file15" \
"${PROG} [a-z]*: scheduling file .first-dir/file15. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"

	  # Now the case where we try to give it a directory which is not
	  # under CVS control.
	  mkdir bogus-dir
	  touch bogus-dir/file16
	  # The first message, from local CVS, is nice.  The second one
	  # is not nice; would be good to fix remote CVS to give a clearer
	  # message (e.g. the one from local CVS).  But at least it is an
	  # error message.
	  dotest_fail errmsg2-16 "${testcvs} add bogus-dir/file16" \
"${PROG} [a-z]*: in directory bogus-dir:
${PROG} \[[a-z]* aborted\]: there is no version here; do .${PROG} checkout. first" \
"${PROG} [a-z]*: cannot open CVS/Entries for reading: No such file or directory
${PROG} \[add aborted\]: no repository"
	  rm -r bogus-dir

	  # One error condition we don't test for is trying to add a file
	  # or directory which already is there.

	  dotest errmsg2-17 "${testcvs} -q ci -m checkin" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file15,v
done
Checking in first-dir/file15;
${TESTDIR}/cvsroot/first-dir/file15,v  <--  file15
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/sdir10/ssdir/ssfile,v
done
Checking in first-dir/sdir10/ssdir/ssfile;
${TESTDIR}/cvsroot/first-dir/sdir10/ssdir/ssfile,v  <--  ssfile
initial revision: 1\.1
done"

	  cd ..
	  rm -r 1
	  rm -rf ${TESTDIR}/cvsroot/first-dir
	  ;;

	devcom)
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  if ${testcvs} -q co first-dir >>${LOGFILE} ; then
	      pass 169
	  else
	      fail 169
	  fi

	  cd first-dir
	  echo abb >abb
	  if ${testcvs} add abb 2>>${LOGFILE}; then
	      pass 170
	  else
	      fail 170
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	      pass 171
	  else
	      fail 171
	  fi
	  dotest_fail 171a0 "${testcvs} watch" "Usage${DOTSTAR}"
	  if ${testcvs} watch on; then
	      pass 172
	  else
	      fail 172
	  fi
	  echo abc >abc
	  if ${testcvs} add abc 2>>${LOGFILE}; then
	      pass 173
	  else
	      fail 173
	  fi
	  if ${testcvs} ci -m added >>${LOGFILE} 2>&1; then
	      pass 174
	  else
	      fail 174
	  fi

	  cd ../..
	  mkdir 2
	  cd 2

	  if ${testcvs} -q co first-dir >>${LOGFILE}; then
	      pass 175
	  else
	      fail 175
	  fi
	  cd first-dir
	  if test -w abb; then
	      fail 176
	  else
	      pass 176
	  fi
	  if test -w abc; then
	      fail 177
	  else
	      pass 177
	  fi

	  if ${testcvs} editors >../ans178.tmp; then
	      pass 178
	  else
	      fail 178
	  fi
	  cat ../ans178.tmp >>${LOGFILE}
	  if test -s ../ans178.tmp; then
	      fail 178a
	  else
	      pass 178a
	  fi

	  if ${testcvs} edit abb; then
	      pass 179
	  else
	      fail 179
	  fi

	  if ${testcvs} editors >../ans180.tmp; then
	      pass 180
	  else
	      fail 180
	  fi
	  cat ../ans180.tmp >>${LOGFILE}
	  if test -s ../ans180.tmp; then
	      pass 181
	  else
	      fail 181
	  fi

	  echo aaaa >>abb
	  if ${testcvs} ci -m modify abb >>${LOGFILE} 2>&1; then
	      pass 182
	  else
	      fail 182
	  fi
	  # Unedit of a file not being edited should be a noop.
	  dotest 182.5 "${testcvs} unedit abb" ''

	  if ${testcvs} editors >../ans183.tmp; then
	      pass 183
	  else
	      fail 183
	  fi
	  cat ../ans183.tmp >>${LOGFILE}
	  if test -s ../ans183.tmp; then
	      fail 184
	  else
	      pass 184
	  fi

	  if test -w abb; then
	      fail 185
	  else
	      pass 185
	  fi

	  if ${testcvs} edit abc; then
	      pass 186a1
	  else
	      fail 186a1
	  fi
	  # Unedit of an unmodified file.
	  if ${testcvs} unedit abc; then
	      pass 186a2
	  else
	      fail 186a2
	  fi
	  if ${testcvs} edit abc; then
	      pass 186a3
	  else
	      fail 186a3
	  fi
	  echo changedabc >abc
	  # Try to unedit a modified file; cvs should ask for confirmation
	  if (echo no | ${testcvs} unedit abc) >>${LOGFILE}; then
	      pass 186a4
	  else
	      fail 186a4
	  fi
	  if echo changedabc | cmp - abc; then
	      pass 186a5
	  else
	      fail 186a5
	  fi
	  # OK, now confirm the unedit
	  if (echo yes | ${testcvs} unedit abc) >>${LOGFILE}; then
	      pass 186a6
	  else
	      fail 186a6
	  fi
	  if echo abc | cmp - abc; then
	      pass 186a7
	  else
	      fail 186a7
	  fi

	  dotest devcom-a0 "${testcvs} watchers" ''

	  # FIXME: This probably should be an error message instead
	  # of silently succeeding and printing nothing.
	  dotest devcom-a-nonexist "${testcvs} watchers nonexist" ''

	  dotest devcom-a1 "${testcvs} watch add" ''
	  dotest devcom-a2 "${testcvs} watchers" \
"abb	${username}	edit	unedit	commit
abc	${username}	edit	unedit	commit"
	  dotest devcom-a3 "${testcvs} watch remove -a unedit abb" ''
	  dotest devcom-a4 "${testcvs} watchers abb" \
"abb	${username}	edit	commit"

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

	  # Test checking out the directory rather than the file.
	  mkdir 3
	  cd 3
	  dotest devcom-t2 "${testcvs} -q co -rtag first-dir" \
'U first-dir/abb
U first-dir/abc'
	  cd ..
	  # Since the files are readonly, use -f.
	  rm -rf 3

	  # Now do it again, after removing the val-tags file created
	  # by devcom-t1 to force CVS to search the repository
	  # containing CVS directories.
	  rm ${CVSROOT_DIRNAME}/CVSROOT/val-tags
	  mkdir 3
	  cd 3
	  dotest devcom-t3 "${testcvs} -q co -rtag first-dir" \
'U first-dir/abb
U first-dir/abc'
	  cd ..
	  # Since the files are readonly, use -f.
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

	  cd first-dir
	  # OK, now we want to try files in various states with cvs edit.
	  dotest devcom2-12 "${testcvs} edit w4" \
"${PROG} edit: no such file w4; ignored"
	  # Try the same thing with a per-directory watch set.
	  dotest devcom2-13 "${testcvs} watch on" ''
	  dotest devcom2-14 "${testcvs} edit w5" \
"${PROG} edit: no such file w5; ignored"
	  dotest devcom2-15 "${testcvs} editors" ''
	  dotest devcom2-16 "${testcvs} editors w4" ''
	  # Make sure there are no droppings lying around
	  dotest devcom2-17 "cat ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr" \
"Fw1	_watched=
Fw2	_watched=
Fw3	_watched=
Fnw1	_watched=
D	_watched="
	  cd ..

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

	watch4)
	  # More watch tests, including adding directories.
	  mkdir 1; cd 1
	  dotest watch4-0a "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest watch4-0b "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"

	  cd first-dir
	  dotest watch4-1 "${testcvs} watch on" ''
	  # This is just like the 173 test
	  touch file1
	  dotest watch4-2 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest watch4-3 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  # Now test the analogous behavior for directories.
	  mkdir subdir
	  dotest watch4-4 "${testcvs} add subdir" \
"Directory ${TESTDIR}/cvsroot/first-dir/subdir added to the repository"
	  cd subdir
	  touch sfile
	  dotest watch4-5 "${testcvs} add sfile" \
"${PROG} [a-z]*: scheduling file .sfile. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest watch4-6 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/subdir/sfile,v
done
Checking in sfile;
${TESTDIR}/cvsroot/first-dir/subdir/sfile,v  <--  sfile
initial revision: 1\.1
done"
	  cd ../../..
	  mkdir 2; cd 2
	  dotest watch4-7 "${testcvs} -q co first-dir" "U first-dir/file1
U first-dir/subdir/sfile"
	  dotest_fail watch4-8 "test -w first-dir/file1" ''
	  dotest_fail watch4-9 "test -w first-dir/subdir/sfile" ''
	  cd first-dir
	  dotest watch4-10 "${testcvs} edit file1" ''
	  echo 'edited in 2' >file1
	  cd ../..

	  cd 1/first-dir
	  dotest watch4-11 "${testcvs} edit file1" ''
	  echo 'edited in 1' >file1
	  dotest watch4-12 "${testcvs} -q ci -m edit-in-1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../..
	  cd 2/first-dir
	  dotest watch4-13 "${testcvs} -q update" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into file1
rcsmerge: warning: conflicts during merge
${PROG} [a-z]*: conflicts found in file1
C file1"
	  if (echo yes | ${testcvs} unedit file1) >>${LOGFILE}; then
	    pass watch4-14
	  else
	    fail watch4-15
	  fi
	  # This could plausibly be defined to either go back to the revision
	  # which was cvs edit'd (the status quo), or back to revision 1.2
	  # (that is, the merge could update CVS/Base/file1).  We pick the
	  # former because it is easier to implement, not because we have
	  # thought much about which is better.
	  dotest watch4-16 "cat file1" ''
	  # Make sure CVS really thinks we are at 1.1.
	  dotest watch4-17 "${testcvs} -q update" "U file1"
	  dotest watch4-18 "cat file1" "edited in 1"
	  cd ../..

	  # As a sanity check, make sure we are in the right place.
	  dotest watch4-cleanup-1 "test -d 1" ''
	  dotest watch4-cleanup-1 "test -d 2" ''
	  # Specify -f because of the readonly files.
	  rm -rf 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	ignore)
	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir wnt
	  cd wnt

	  dotest 187a1 "${testcvs} -q co CVSROOT" "U CVSROOT/${DOTSTAR}"
	  cd CVSROOT
	  echo rootig.c >cvsignore
	  dotest 187a2 "${testcvs} add cvsignore" "${PROG}"' [a-z]*: scheduling file `cvsignore'"'"' for addition
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'

	  # As of Jan 96, local CVS prints "Examining ." and remote doesn't.
	  # Accept either.
	  dotest 187a3 " ${testcvs} ci -m added" \
"${PROG} [a-z]*: Examining \.
RCS file: ${TESTDIR}/cvsroot/CVSROOT/cvsignore,v
done
Checking in cvsignore;
${TESTDIR}/cvsroot/CVSROOT/cvsignore,v  <--  cvsignore
initial revision: 1\.1
done
${PROG} [a-z]*: Rebuilding administrative file database"

	  cd ..
	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	      pass 187a4
	  else
	      fail 187a4
	  fi

	  # CVS looks at the home dir from getpwuid, not HOME (is that correct
	  # behavior?), so this is hard to test and we won't try.
	  # echo foobar.c >${HOME}/.cvsignore
	  CVSIGNORE=envig.c; export CVSIGNORE
	  mkdir dir-to-import
	  cd dir-to-import
	  touch foobar.c bar.c rootig.c defig.o envig.c optig.c
	  # We use sort because we can't predict the order in which
	  # the files will be listed.
	  dotest_sort 188a "${testcvs} import -m m -I optig.c first-dir tag1 tag2" \
'

I first-dir/defig.o
I first-dir/envig.c
I first-dir/optig.c
I first-dir/rootig.c
N first-dir/bar.c
N first-dir/foobar.c
No conflicts created by this import'
	  dotest_sort 188b "${testcvs} import -m m -I ! second-dir tag3 tag4" \
'

N second-dir/bar.c
N second-dir/defig.o
N second-dir/envig.c
N second-dir/foobar.c
N second-dir/optig.c
N second-dir/rootig.c
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
	  dotest_sort 189d "${testcvs} -q update -I ! -I CVS" \
"${QUESTION} defig.o
${QUESTION} envig.c
${QUESTION} notig.c
${QUESTION} optig.c
${QUESTION} rootig.c"

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
	  dotest_sort 190 "${testcvs} -qn update" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c"
	  dotest_sort 191 "${testcvs} -qn update -I! -I CVS" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} first-dir/defig.o
${QUESTION} first-dir/envig.c
${QUESTION} first-dir/rootig.c
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c"

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
	  cd ..
	  rm -r wnt
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
${PROG} [a-z]*: nonmergeable file needs merge
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

	  dotest binfiles-9 "${testcvs} -q update -A" ''
	  dotest binfiles-10 "${testcvs} -q update -kk" '[UP] binfile'
	  dotest binfiles-11 "${testcvs} -q update" ''
	  dotest binfiles-12 "${testcvs} -q update -A" '[UP] binfile'
	  dotest binfiles-13 "${testcvs} -q update -A" ''

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
"${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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

	  # Now test that -A can clear the sticky option.
	  dotest binfiles-sticky5 "${testcvs} -q update -A nibfile" \
"[UP] nibfile"
	  dotest binfiles-sticky6 "${testcvs} -q status nibfile" \
"===================================================================
File: nibfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/nibfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest binfiles-15 "${testcvs} -q admin -kb nibfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/nibfile,v
done"
	  dotest binfiles-16 "${testcvs} -q update nibfile" "[UP] nibfile"
	  dotest binfiles-17 "${testcvs} -q status nibfile" \
"===================================================================
File: nibfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${TESTDIR}/cvsroot/first-dir/nibfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"

	  dotest binfiles-o1 "${testcvs} admin -o1.3:: binfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/binfile,v
deleting revision 1\.5
deleting revision 1\.4
done"
	  dotest binfiles-o2 "${testcvs} admin -o::1.3 binfile" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/binfile,v
deleting revision 1\.2
deleting revision 1\.1
done"
	  dotest binfiles-o3 "${testcvs} -q log -h -N binfile" "
RCS file: ${TESTDIR}/cvsroot/first-dir/binfile,v
Working file: binfile
head: 1\.3
branch:
locks: strict
access list:
keyword substitution: v
total revisions: 1
============================================================================="

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
	  #
	  # Cases (we are merging from the branch to the trunk):
	  # binfile.dat) File added on branch, not on trunk.
	  #      File should be marked for addition.
	  # brmod) File modified on branch, not on trunk.
	  #      File should be copied over to trunk (no merging is needed).
	  # brmod-trmod) File modified on branch, also on trunk.
	  #      This is a conflict.  Present the user with both files and
	  #      let them figure it out.
	  # brmod-wdmod) File modified on branch, not modified in the trunk
	  #      repository, but modified in the (trunk) working directory.
	  #      This is also a conflict.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest binfiles2-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # The most important thing here is that binfile, binfile2, &c
	  # each be distinct from each other.  We also make sure to include
	  # a few likely end-of-line patterns to make sure nothing is
	  # being munged as if in text mode.
	  awk 'BEGIN { printf "%c%c%c%c%c%c", 2, 10, 137, 0, 13, 10 }' \
	    </dev/null >../binfile
	  cat ../binfile ../binfile >../binfile2
	  cat ../binfile2 ../binfile >../binfile3

	  # FIXCVS: unless a branch has at least one file on it,
	  # tag_check_valid won't know it exists.  So if brmod didn't
	  # exist, we would have to invent it.
	  cp ../binfile brmod
	  cp ../binfile brmod-trmod
	  cp ../binfile brmod-wdmod
	  dotest binfiles2-1a \
"${testcvs} add -kb brmod brmod-trmod brmod-wdmod" \
"${PROG} [a-z]*: scheduling file .brmod. for addition
${PROG} [a-z]*: scheduling file .brmod-trmod. for addition
${PROG} [a-z]*: scheduling file .brmod-wdmod. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest binfiles2-1b "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/brmod,v
done
Checking in brmod;
${TESTDIR}/cvsroot/first-dir/brmod,v  <--  brmod
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v
done
Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v
done
Checking in brmod-wdmod;
${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v  <--  brmod-wdmod
initial revision: 1\.1
done"
	  dotest binfiles2-2 "${testcvs} -q tag -b br" 'T brmod
T brmod-trmod
T brmod-wdmod'
	  dotest binfiles2-3 "${testcvs} -q update -r br" ''
	  cp ../binfile binfile.dat
	  dotest binfiles2-4 "${testcvs} add -kb binfile.dat" \
"${PROG} [a-z]*: scheduling file .binfile\.dat. for addition on branch .br.
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  cp ../binfile2 brmod
	  cp ../binfile2 brmod-trmod
	  cp ../binfile2 brmod-wdmod
	  dotest binfiles2-5 "${testcvs} -q ci -m br-changes" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/Attic/binfile\.dat,v
done
Checking in binfile\.dat;
${TESTDIR}/cvsroot/first-dir/Attic/binfile\.dat,v  <--  binfile\.dat
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod;
${TESTDIR}/cvsroot/first-dir/brmod,v  <--  brmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-wdmod;
${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest binfiles2-6 "${testcvs} -q update -A" \
"${PROG} [a-z]*: warning: binfile\.dat is not (any longer) pertinent
[UP] brmod
[UP] brmod-trmod
[UP] brmod-wdmod"
	  dotest_fail binfiles2-7 "test -f binfile.dat" ''
	  dotest binfiles2-7-brmod "cmp ../binfile brmod"
	  cp ../binfile3 brmod-trmod
	  dotest binfiles2-7a "${testcvs} -q ci -m tr-modify" \
"Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.2; previous revision: 1\.1
done"
	  cp ../binfile3 brmod-wdmod

	  dotest binfiles2-8 "${testcvs} -q update -j br" \
"U binfile\.dat
U brmod
${PROG} [a-z]*: nonmergeable file needs merge
${PROG} [a-z]*: revision 1.1.2.1 from repository is now in brmod-trmod
${PROG} [a-z]*: file from working directory is now in .#brmod-trmod.1.2
C brmod-trmod
M brmod-wdmod
${PROG} [a-z]*: nonmergeable file needs merge
${PROG} [a-z]*: revision 1.1.2.1 from repository is now in brmod-wdmod
${PROG} [a-z]*: file from working directory is now in .#brmod-wdmod.1.1
C brmod-wdmod"

	  dotest binfiles2-9 "cmp ../binfile binfile.dat"
	  dotest binfiles2-9-brmod "cmp ../binfile2 brmod"
	  dotest binfiles2-9-brmod-trmod "cmp ../binfile2 brmod-trmod"
	  dotest binfiles2-9-brmod-trmod "cmp ../binfile2 brmod-wdmod"
	  dotest binfiles2-9a-brmod-trmod "cmp ../binfile3 .#brmod-trmod.1.2"
	  dotest binfiles2-9a-brmod-wdmod "cmp ../binfile3 .#brmod-wdmod.1.1"

	  # Test that everything was properly scheduled.
	  dotest binfiles2-10 "${testcvs} -q ci -m checkin" \
"Checking in binfile\.dat;
${TESTDIR}/cvsroot/first-dir/binfile\.dat,v  <--  binfile\.dat
new revision: 1\.2; previous revision: 1\.1
done
Checking in brmod;
${TESTDIR}/cvsroot/first-dir/brmod,v  <--  brmod
new revision: 1\.2; previous revision: 1\.1
done
Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.3; previous revision: 1\.2
done
Checking in brmod-wdmod;
${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.2; previous revision: 1\.1
done"

	  dotest_fail binfiles2-o1 "${testcvs} -q admin -o :1.2 brmod-trmod" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v
deleting revision 1\.2
${PROG} [a-z]*: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v: can't remove branch point 1\.1
${PROG} [a-z]*: cannot modify RCS file for .brmod-trmod."
	  dotest binfiles2-o2 "${testcvs} -q admin -o 1.1.2.1: brmod-trmod" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v
deleting revision 1\.1\.2\.1
done"
	  dotest binfiles2-o3 "${testcvs} -q admin -o :1.2 brmod-trmod" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v
deleting revision 1\.2
deleting revision 1\.1
done"
	  dotest binfiles2-o4 "${testcvs} -q log -N brmod-trmod" "
RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v
Working file: brmod-trmod
head: 1\.3
branch:
locks: strict
access list:
keyword substitution: b
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.3
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
checkin
============================================================================="
	  cd ..
	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 1
	  ;;

	mcopy)
	  # See comment at "mwrap" test for list of other wrappers tests.
	  # Test cvs's ability to handle nonmergeable files specified with
	  # -m 'COPY' in wrappers.  Similar to the binfiles2 test,
	  # which tests the same thing for binary files
	  # (which are non-mergeable in the same sense).
	  #
	  # Cases (we are merging from the branch to the trunk):
	  # brmod) File modified on branch, not on trunk.
	  #      File should be copied over to trunk (no merging is needed).
	  # brmod-trmod) File modified on branch, also on trunk.
	  #      This is a conflict.  Present the user with both files and
	  #      let them figure it out.
	  # brmod-wdmod) File modified on branch, not modified in the trunk
	  #      repository, but modified in the (trunk) working directory.
	  #      This is also a conflict.

	  # For the moment, remote CVS can't pass wrappers from CVSWRAPPERS
	  # (see wrap_send).  So skip these tests for remote.
	  if test "x$remote" = xno; then

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest mcopy-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # FIXCVS: unless a branch has at least one file on it,
	  # tag_check_valid won't know it exists.  So if brmod didn't
	  # exist, we would have to invent it.
	  echo 'brmod initial contents' >brmod
	  echo 'brmod-trmod initial contents' >brmod-trmod
	  echo 'brmod-wdmod initial contents' >brmod-wdmod
	  echo "* -m 'COPY'" >.cvswrappers
	  dotest mcopy-1a \
"${testcvs} add .cvswrappers brmod brmod-trmod brmod-wdmod" \
"${PROG} [a-z]*: scheduling file .\.cvswrappers. for addition
${PROG} [a-z]*: scheduling file .brmod. for addition
${PROG} [a-z]*: scheduling file .brmod-trmod. for addition
${PROG} [a-z]*: scheduling file .brmod-wdmod. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest mcopy-1b "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/\.cvswrappers,v
done
Checking in \.cvswrappers;
${TESTDIR}/cvsroot/first-dir/\.cvswrappers,v  <--  \.cvswrappers
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/brmod,v
done
Checking in brmod;
${TESTDIR}/cvsroot/first-dir/brmod,v  <--  brmod
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-trmod,v
done
Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v
done
Checking in brmod-wdmod;
${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v  <--  brmod-wdmod
initial revision: 1\.1
done"

	  # NOTE: .cvswrappers files are broken (see comment in
	  # src/wrapper.c).  So doing everything via the environment
	  # variable is a workaround.  Better would be to test them
	  # both.
	  CVSWRAPPERS="* -m 'COPY'"
	  export CVSWRAPPERS
	  dotest mcopy-2 "${testcvs} -q tag -b br" 'T \.cvswrappers
T brmod
T brmod-trmod
T brmod-wdmod'
	  dotest mcopy-3 "${testcvs} -q update -r br" ''
	  echo 'modify brmod on br' >brmod
	  echo 'modify brmod-trmod on br' >brmod-trmod
	  echo 'modify brmod-wdmod on br' >brmod-wdmod
	  dotest mcopy-5 "${testcvs} -q ci -m br-changes" \
"Checking in brmod;
${TESTDIR}/cvsroot/first-dir/brmod,v  <--  brmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-wdmod;
${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest mcopy-6 "${testcvs} -q update -A" \
"[UP] brmod
[UP] brmod-trmod
[UP] brmod-wdmod"
	  dotest mcopy-7 "cat brmod brmod-trmod brmod-wdmod" \
"brmod initial contents
brmod-trmod initial contents
brmod-wdmod initial contents"

	  echo 'modify brmod-trmod again on trunk' >brmod-trmod
	  dotest mcopy-7a "${testcvs} -q ci -m tr-modify" \
"Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.2; previous revision: 1\.1
done"
	  echo 'modify brmod-wdmod in working dir' >brmod-wdmod

	  dotest mcopy-8 "${testcvs} -q update -j br" \
"U brmod
${PROG} [a-z]*: nonmergeable file needs merge
${PROG} [a-z]*: revision 1.1.2.1 from repository is now in brmod-trmod
${PROG} [a-z]*: file from working directory is now in .#brmod-trmod.1.2
C brmod-trmod
M brmod-wdmod
${PROG} [a-z]*: nonmergeable file needs merge
${PROG} [a-z]*: revision 1.1.2.1 from repository is now in brmod-wdmod
${PROG} [a-z]*: file from working directory is now in .#brmod-wdmod.1.1
C brmod-wdmod"

	  dotest mcopy-9 "cat brmod brmod-trmod brmod-wdmod" \
"modify brmod on br
modify brmod-trmod on br
modify brmod-wdmod on br"
	  dotest mcopy-9a "cat .#brmod-trmod.1.2 .#brmod-wdmod.1.1" \
"modify brmod-trmod again on trunk
modify brmod-wdmod in working dir"

	  # Test that everything was properly scheduled.
	  dotest mcopy-10 "${testcvs} -q ci -m checkin" \
"Checking in brmod;
${TESTDIR}/cvsroot/first-dir/brmod,v  <--  brmod
new revision: 1\.2; previous revision: 1\.1
done
Checking in brmod-trmod;
${TESTDIR}/cvsroot/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.3; previous revision: 1\.2
done
Checking in brmod-wdmod;
${TESTDIR}/cvsroot/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ..
	  cd ..

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 1
	  unset CVSWRAPPERS

	  fi # end of tests to be skipped for remote

	  ;;

	binwrap)
	  # Test the ability to specify binary-ness based on file name.
	  # See "mwrap" for a list of other wrappers tests.

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

	binwrap2)
	  # Test the ability to specify binary-ness based on file name.
	  # See "mwrap" for a list of other wrappers tests.

	  mkdir dir-to-import
	  cd dir-to-import
	  touch foo.c foo.exe

	  # Specify that all files are binary except *.c.
	  # The order seems to matter, with the earlier rules taking
	  # precedence.  I'm not sure whether that is good or not,
	  # but it is the current behavior.
	  if ${testcvs} import -m message -I ! \
	      -W "*.c -k 'o'" -W "* -k 'b'" \
	      first-dir tag1 tag2 >>${LOGFILE}; then
	    pass binwrap2-1
	  else
	    fail binwrap2-1
	  fi
	  cd ..
	  rm -r dir-to-import
	  dotest binwrap2-2 "${testcvs} -q co first-dir" 'U first-dir/foo.c
U first-dir/foo.exe'
	  dotest binwrap2-3 "${testcvs} -q status first-dir" \
"===================================================================
File: foo\.c            	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	${TESTDIR}/cvsroot/first-dir/foo\.c,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-ko

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

        binwrap3)
          # Test communication of file-specified -k wrappers between
          # client and server, in `import':
          #
          #   1. Set up a directory tree, populate it with files.
          #   2. Give each directory a different .cvswrappers file. 
          #   3. Give the server its own .cvswrappers file.
          #   4. Import the whole tree, see if the right files got set
          #      to binary.
          #
          # The tree has a top ("0th") level, and two subdirs, sub1/
          # and sub2/; sub2/ contains directory subsub/.  Every
          # directory has a .cvswrappers file as well as regular
          # files.
          #
          # In the file names, "foo-b.*" should end up binary, and
          # "foo-t.*" should end up text.  Don't worry about the two
          # letter extensions; they're just there to help me keep
          # things straight.
          #
          # Here's the directory tree:
          #
          # ./
          #    .cvswrappers
          #    foo-b.c0
          #    foo-b.sb
          #    foo-t.c1
          #    foo-t.st
          #
          #    sub1/             sub2/
          #      .cvswrappers      .cvswrappers
          #      foo-b.c1          foo-b.sb
          #      foo-b.sb          foo-b.st
          #      foo-t.c0          foo-t.c0
          #      foo-t.st          foo-t.c1
          #                        foo-t.c2
          #                        foo-t.c3
          #
          #                        subsub/
          #                          .cvswrappers
          #                          foo-b.c3
          #                          foo-b.sb
          #                          foo-t.c0
          #                          foo-t.c1
          #                          foo-t.c2
          #                          foo-t.st

          binwrap3_line1="This is a test file "
          binwrap3_line2="containing little of use "
          binwrap3_line3="except this non-haiku"

          binwrap3_text="${binwrap3_line1}${binwrap3_line2}${binwrap3_line3}"

          cd ${TESTDIR}

	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir wnt
	  cd wnt

          mkdir binwrap3 # the 0th dir
          mkdir binwrap3/sub1
          mkdir binwrap3/sub2
          mkdir binwrap3/sub2/subsub
          
          echo "*.c0 -k 'b'" > binwrap3/.cvswrappers
          echo "whatever -k 'b'" >> binwrap3/.cvswrappers
          echo ${binwrap3_text} > binwrap3/foo-b.c0
          echo ${binwrap3_text} > binwrap3/foo-b.sb
          echo ${binwrap3_text} > binwrap3/foo-t.c1
          echo ${binwrap3_text} > binwrap3/foo-t.st

          echo "*.c1 -k 'b'" > binwrap3/sub1/.cvswrappers
          echo "whatever -k 'b'" >> binwrap3/sub1/.cvswrappers
          echo ${binwrap3_text} > binwrap3/sub1/foo-b.c1
          echo ${binwrap3_text} > binwrap3/sub1/foo-b.sb
          echo ${binwrap3_text} > binwrap3/sub1/foo-t.c0
          echo ${binwrap3_text} > binwrap3/sub1/foo-t.st

          echo "*.st -k 'b'" > binwrap3/sub2/.cvswrappers
          echo ${binwrap3_text} > binwrap3/sub2/foo-b.sb
          echo ${binwrap3_text} > binwrap3/sub2/foo-b.st
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c0
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c1
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c2
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c3

          echo "*.c3 -k 'b'" > binwrap3/sub2/subsub/.cvswrappers
          echo "foo -k 'b'" >> binwrap3/sub2/subsub/.cvswrappers
          echo "c0* -k 'b'" >> binwrap3/sub2/subsub/.cvswrappers
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-b.c3
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-b.sb
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-t.c0
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-t.c1
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-t.c2
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-t.st

          # Now set up CVSROOT/cvswrappers, the easy way:
	  dotest binwrap3-1 "${testcvs} -q co CVSROOT" "[UP] CVSROOT${DOTSTAR}"
	  cd CVSROOT
          # This destroys anything currently in cvswrappers, but
	  # presumably other tests will take care of it themselves if
	  # they use cvswrappers:
	  echo "foo*.sb  -k 'b'" > cvswrappers
	  dotest binwrap3-2 "${testcvs} -q ci -m cvswrappers-mod" \
"Checking in cvswrappers;
${TESTDIR}/cvsroot/CVSROOT/cvswrappers,v  <--  cvswrappers
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
          cd ..

          # Avoid environmental interference
          CVSWRAPPERS_SAVED=${CVSWRAPPERS}
          unset CVSWRAPPERS

          # Do the import
          cd binwrap3
	  # Not importing .cvswrappers tests whether the client is really
	  # letting the server know "honestly" whether the file is binary,
	  # rather than just letting the server see the .cvswrappers file.
          dotest binwrap3-2a \
"${testcvs} import -m . -I .cvswrappers binwrap3 tag1 tag2" \
"[NI] ${DOTSTAR}"

	  # OK, now test "cvs add".
          cd ..
	  rm -r binwrap3
          dotest binwrap3-2b "${testcvs} co binwrap3" "${DOTSTAR}"
          cd binwrap3
	  cd sub2
	  echo "*.newbin -k 'b'" > .cvswrappers
	  echo .cvswrappers >.cvsignore
	  echo .cvsignore >>.cvsignore
	  touch file1.newbin file1.txt
	  dotest binwrap3-2c "${testcvs} add file1.newbin file1.txt" \
"${PROG} [a-z]*: scheduling file .file1\.newbin. for addition
${PROG} [a-z]*: scheduling file .file1\.txt. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest binwrap3-2d "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/binwrap3/sub2/file1\.newbin,v
done
Checking in file1\.newbin;
${TESTDIR}/cvsroot/binwrap3/sub2/file1\.newbin,v  <--  file1\.newbin
initial revision: 1\.1
done
RCS file: ${TESTDIR}/cvsroot/binwrap3/sub2/file1\.txt,v
done
Checking in file1\.txt;
${TESTDIR}/cvsroot/binwrap3/sub2/file1\.txt,v  <--  file1\.txt
initial revision: 1\.1
done"
	  cd ..

          # Now check out the module and see which files are binary.
          cd ..
	  rm -r binwrap3
          dotest binwrap3-3 "${testcvs} co binwrap3" "${DOTSTAR}"
          cd binwrap3

          # Running "cvs status" and matching output is too
          # error-prone, too likely to falsely fail.  Instead, we'll
          # just grep the Entries lines:

          dotest binwrap3-top1 "grep foo-b.c0 ./CVS/Entries" \
                 "/foo-b.c0/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-top2 "grep foo-b.sb ./CVS/Entries" \
                 "/foo-b.sb/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-top3 "grep foo-t.c1 ./CVS/Entries" \
                 "/foo-t.c1/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-top4 "grep foo-t.st ./CVS/Entries" \
                 "/foo-t.st/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub1-1 "grep foo-b.c1 sub1/CVS/Entries" \
                 "/foo-b.c1/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-sub1-2 "grep foo-b.sb sub1/CVS/Entries" \
                 "/foo-b.sb/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-sub1-3 "grep foo-t.c0 sub1/CVS/Entries" \
                 "/foo-t.c0/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub1-4 "grep foo-t.st sub1/CVS/Entries" \
                 "/foo-t.st/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub2-1 "grep foo-b.sb sub2/CVS/Entries" \
                 "/foo-b.sb/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-sub2-2 "grep foo-b.st sub2/CVS/Entries" \
                 "/foo-b.st/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-sub2-3 "grep foo-t.c0 sub2/CVS/Entries" \
                 "/foo-t.c0/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub2-4 "grep foo-t.c1 sub2/CVS/Entries" \
                 "/foo-t.c1/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub2-5 "grep foo-t.c2 sub2/CVS/Entries" \
                 "/foo-t.c2/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub2-6 "grep foo-t.c3 sub2/CVS/Entries" \
                 "/foo-t.c3/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-subsub1 "grep foo-b.c3 sub2/subsub/CVS/Entries" \
                 "/foo-b.c3/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-subsub2 "grep foo-b.sb sub2/subsub/CVS/Entries" \
                 "/foo-b.sb/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-subsub3 "grep foo-t.c0 sub2/subsub/CVS/Entries" \
                 "/foo-t.c0/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-subsub4 "grep foo-t.c1 sub2/subsub/CVS/Entries" \
                 "/foo-t.c1/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-subsub5 "grep foo-t.c2 sub2/subsub/CVS/Entries" \
                 "/foo-t.c2/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-subsub6 "grep foo-t.st sub2/subsub/CVS/Entries" \
                 "/foo-t.st/1.1.1.1/[A-Za-z0-9 	:]*//"

	  dotest binwrap3-sub2-add1 "grep file1.newbin sub2/CVS/Entries" \
	    "/file1.newbin/1.1/[A-Za-z0-9 	:]*/-kb/"
	  dotest binwrap3-sub2-add2 "grep file1.txt sub2/CVS/Entries" \
	    "/file1.txt/1.1/[A-Za-z0-9 	:]*//"

          # Restore and clean up
          cd ..
	  rm -r binwrap3 CVSROOT
	  cd ..
	  rm -r wnt
	  rm -rf ${CVSROOT_DIRNAME}/binwrap3
          CVSWRAPPERS=${CVSWRAPPERS_SAVED}
          ;; 

	mwrap)
	  # Tests of various wrappers features:
	  # -m 'COPY' and cvs update: mwrap
	  # -m 'COPY' and joining: mcopy
	  # -k: binwrap, binwrap2
	  # -t/-f: hasn't been written yet.
	  # 
	  # Tests of different ways of specifying wrappers:
	  # CVSROOT/cvswrappers: mwrap
	  # -W: binwrap, binwrap2
	  # .cvswrappers in working directory, local: mcopy
	  # CVSROOT/cvswrappers, .cvswrappers remote: binwrap3
	  # CVSWRAPPERS environment variable: mcopy

	  # This test is similar to binfiles-con1; -m 'COPY' specifies
	  # non-mergeableness the same way that -kb does.

	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir wnt
	  cd wnt

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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
	  dotest mwrap-8 "${testcvs} -q update" \
"U aa
${PROG} [a-z]*: nonmergeable file needs merge
${PROG} [a-z]*: revision 1\.2 from repository is now in aa
${PROG} [a-z]*: file from working directory is now in \.#aa\.1\.1
C aa"
	  dotest mwrap-9 "cat aa" "changed in m2"
	  dotest mwrap-10 "cat .#aa.1.1" "changed in m1"
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
	  cd ..
	  rm -r wnt
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	info)
	  # Administrative file tests.
	  # Here is a list of where each administrative file is tested:
	  # loginfo: info
	  # modules: modules, modules2, modules3
	  # cvsignore: ignore
	  # verifymsg: info
	  # cvswrappers: mwrap
	  # config: config

	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir wnt
	  cd wnt

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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'

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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
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
	  dotest info-9 "cat $TESTDIR/testlog" "xenv-valueyz=${username}=${TESTDIR}/cvsroot="
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
#!${TESTSHELL}
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
	  cd ..
	  rm -r wnt
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	config)
	  # Tests of the CVSROOT/config file.  See the comment at the
	  # "info" tests for a full list of administrative file tests.

	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir wnt
	  cd wnt

	  dotest config-1 "${testcvs} -q co CVSROOT" "U CVSROOT/${DOTSTAR}"
	  cd CVSROOT
	  echo 'bogus line' >config
	  dotest config-3 "${testcvs} -q ci -m change-to-bogus-line" \
"Checking in config;
${TESTDIR}/cvsroot/CVSROOT/config,v  <--  config
new revision: 1\.2; previous revision: 1\.1
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  echo 'BogusOption=yes' >config
	  dotest config-4 "${testcvs} -q ci -m change-to-bogus-opt" \
"${PROG} [a-z]*: syntax error in ${TESTDIR}/cvsroot/CVSROOT/config: line 'bogus line' is missing '='
Checking in config;
${TESTDIR}/cvsroot/CVSROOT/config,v  <--  config
new revision: 1\.3; previous revision: 1\.2
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  echo '# No config is a good config' > config
	  dotest config-5 "${testcvs} -q ci -m change-to-comment" \
"${PROG} [a-z]*: ${TESTDIR}/cvsroot/CVSROOT/config: unrecognized keyword 'BogusOption'
Checking in config;
${TESTDIR}/cvsroot/CVSROOT/config,v  <--  config
new revision: 1\.4; previous revision: 1\.3
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  dotest config-6 "${testcvs} -q update" ''

	  cd ..
	  rm -r CVSROOT
	  cd ..
	  rm -r wnt
	  ;;

	serverpatch)
	  # Test remote CVS handling of unpatchable files.  This isn't
	  # much of a test for local CVS.
	  # We test this with some keyword expansion games, but the situation
	  # also arises if the user modifies the file while CVS is running.
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
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'

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
	  # See also branches-14.3 for logging with a branch off of a branch.
	  # See also multibranch-14 for logging with several branches off the
	  #   same branchpoint.
	  # Tests of each option to cvs log:
	  #   -h: admin-19a-log
	  #   -N: log, log2, admin-19a-log
	  #   -b, -r: log
	  #   -d: rcs

	  # Check in a file with a few revisions and branches.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest log-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 'first revision' > file1
	  dotest log-2 "${testcvs} add file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'

	  # While we're at it, check multi-line comments, input from file,
	  # and trailing whitespace trimming
	  echo 'line 1     '	 >${TESTDIR}/comment.tmp
	  echo '     '		>>${TESTDIR}/comment.tmp
	  echo 'line 2	'	>>${TESTDIR}/comment.tmp
	  echo '	'	>>${TESTDIR}/comment.tmp
	  echo '  	  '	>>${TESTDIR}/comment.tmp
	  dotest log-3 "${testcvs} -q commit -F ${TESTDIR}/comment.tmp" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  rm -f ${TESTDIR}/comment.tmp

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
line 1

line 2"
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

	  dotest log-o0 "${testcvs} admin -o 1.2.2.2:: file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest log-o1 "${testcvs} admin -o ::1.2.2.1 file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest log-o2 "${testcvs} admin -o 1.2.2.1:: file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
deleting revision 1\.2\.2\.2
done"
	  dotest log-o3 "${testcvs} log file1" \
"${log_header}
${log_tags}
${log_header2}
total revisions: 4;	selected revisions: 4
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev1b}
${log_trailer}"
	  dotest log-o4 "${testcvs} -q update -p -r 1.2.2.1 file1" \
"first branch revision"
	  cd ..
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	log2)
	  # More "cvs log" tests, for example the file description.

	  # Check in a file
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest log2-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 'first revision' > file1
	  dotest log2-2 "${testcvs} add -m file1-is-for-testing file1" \
"${PROG}"' [a-z]*: scheduling file `file1'\'' for addition
'"${PROG}"' [a-z]*: use .'"${PROG}"' commit. to add this file permanently'
	  dotest log2-3 "${testcvs} -q commit -m 1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  # Setting the file description with add -m doesn't yet work
	  # client/server, so skip log2-4 for remote.
	  if test "x$remote" = xno; then

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

	  fi # end of tests skipped for remote

	  dotest log2-5 "${testcvs} admin -t-change-description file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest log2-6 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
change-description
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
1
============================================================================="

	  # I believe that in Real Life (TM), this is broken for remote.
	  # That is, the filename in question must be the filename of a
	  # file on the server.  It only happens to work here because the
	  # client machine and the server machine are one and the same.
	  echo 'longer description' >${TESTDIR}/descrip
	  echo 'with two lines' >>${TESTDIR}/descrip
	  dotest log2-7 "${testcvs} admin -t${TESTDIR}/descrip file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest log2-8 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
longer description
with two lines
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
1
============================================================================="

	  # Reading the description from stdin is broken for remote.
	  # See comments in cvs.texinfo for a few more notes on this.
	  if test "x$remote" = xno; then

	    if echo change from stdin | ${testcvs} admin -t -q file1
	    then
	      pass log2-9
	    else
	      fail log2-9
	    fi
	    dotest log2-10 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
change from stdin
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
1
============================================================================="

	  fi # end of tests skipped for remote

	  cd ..
	  rm ${TESTDIR}/descrip
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir

	  ;;

	ann)
	  # Tests of "cvs annotate".  See also basica-10.
	  mkdir 1; cd 1
	  dotest ann-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest ann-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  cat >file1 <<EOF
this
is
the
ancestral
file
EOF
	  dotest ann-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest ann-4 "${testcvs} -q ci -m add file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cat >file1 <<EOF
this
is
a
file

with
a
blank
line
EOF
	  dotest ann-5 "${testcvs} -q ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest ann-6 "${testcvs} -q tag -b br" "T file1"
	  cat >file1 <<EOF
this
is
a
trunk file

with
a
blank
line
EOF
	  dotest ann-7 "${testcvs} -q ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest ann-8 "${testcvs} -q update -r br" "[UP] file1"
	  cat >file1 <<EOF
this
is
a
file

with
a
blank
line
and some
branched content
EOF
	  dotest ann-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"
	  # Note that this annotates the trunk despite the presence
	  # of a sticky tag in the current directory.  This is
	  # fairly bogus, but it is the longstanding behavior for
	  # whatever that is worth.
	  dotest ann-10 "${testcvs} ann" \
"Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          (${username} *[0-9a-zA-Z-]*): this
1\.1          (${username} *[0-9a-zA-Z-]*): is
1\.2          (${username} *[0-9a-zA-Z-]*): a
1\.3          (${username} *[0-9a-zA-Z-]*): trunk file
1\.2          (${username} *[0-9a-zA-Z-]*): 
1\.2          (${username} *[0-9a-zA-Z-]*): with
1\.2          (${username} *[0-9a-zA-Z-]*): a
1\.2          (${username} *[0-9a-zA-Z-]*): blank
1\.2          (${username} *[0-9a-zA-Z-]*): line"
	  dotest ann-11 "${testcvs} ann -r br" \
"Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          (${username} *[0-9a-zA-Z-]*): this
1\.1          (${username} *[0-9a-zA-Z-]*): is
1\.2          (${username} *[0-9a-zA-Z-]*): a
1\.1          (${username} *[0-9a-zA-Z-]*): file
1\.2          (${username} *[0-9a-zA-Z-]*): 
1\.2          (${username} *[0-9a-zA-Z-]*): with
1\.2          (${username} *[0-9a-zA-Z-]*): a
1\.2          (${username} *[0-9a-zA-Z-]*): blank
1\.2          (${username} *[0-9a-zA-Z-]*): line
1\.2\.2\.1      (${username} *[0-9a-zA-Z-]*): and some
1\.2\.2\.1      (${username} *[0-9a-zA-Z-]*): branched content"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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

	    CREREPOS_ROOT=${TESTDIR}/crerepos

	  else
	    # For remote, just create the repository.  We don't yet do
	    # the various other tests above for remote but that should be
	    # changed.
	    mkdir crerepos
	    mkdir crerepos/CVSROOT

	    CREREPOS_ROOT=:ext:`hostname`:${TESTDIR}/crerepos

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

	  # Now test mixing repositories.  This kind of thing tends to
	  # happen accidentally when people work with several repositories.
	  mkdir 1; cd 1
	  dotest crerepos-8 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest crerepos-9 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  touch file1
	  dotest crerepos-10 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest crerepos-11 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  mkdir 1; cd 1
	  dotest crerepos-12 "${testcvs} -d ${CREREPOS_ROOT} -q co -l ." ''
	  mkdir crerepos-dir
	  dotest crerepos-13 "${testcvs} add crerepos-dir" \
"Directory ${TESTDIR}/crerepos/crerepos-dir added to the repository"
	  cd crerepos-dir
	  touch cfile
	  dotest crerepos-14 "${testcvs} add cfile" \
"${PROG} [a-z]*: scheduling file .cfile. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest crerepos-15 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/crerepos/crerepos-dir/cfile,v
done
Checking in cfile;
${TESTDIR}/crerepos/crerepos-dir/cfile,v  <--  cfile
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  mkdir 1; cd 1
	  dotest crerepos-16 "${testcvs} co first-dir" \
"${PROG} [a-z]*: Updating first-dir
U first-dir/file1"
	  dotest crerepos-17 "${testcvs} -d ${CREREPOS_ROOT} co crerepos-dir" \
"${PROG} [a-z]*: Updating crerepos-dir
U crerepos-dir/cfile"

	  if test x`cat CVS/Repository` = x.; then
	    # RELATIVE_REPOS
	    # Fatal error so that we don't go traipsing through the
	    # directories which happen to have the same names from the
	    # wrong repository.
	    dotest_fail crerepos-18 "${testcvs} -q update" \
"${PROG} \[[a-z]* aborted\]: cannot open directory ${TESTDIR}/cvsroot/crerepos-dir: .*" ''
	  else
	    if test "$remote" = no; then
	      # The lack of an error doesn't mean CVS is really
	      # working (things are getting logged to the wrong
	      # history file and such).
	      dotest crerepos-18 "${testcvs} -q update" ''
	    else
	      # Fatal error so that we don't go traipsing through the
	      # directories which happen to have the same names from the
	      # wrong repository.
	      dotest_fail crerepos-18 "${testcvs} -q update" \
"protocol error: directory .${TESTDIR}/crerepos/crerepos-dir. not within root .${TESTDIR}/cvsroot."
	    fi
	  fi

	  cd ..

	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir ${TESTDIR}/crerepos
	  ;;

	rcs)
	  # Test ability to import an RCS file.  Note that this format
	  # is fixed--files written by RCS5, and other software which
	  # implements this format, will be out there "forever" and
	  # CVS must always be able to import such files.

	  # See tests admin-13, admin-25 and rcs-8a for exporting RCS files.

	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  # Currently the way to import an RCS file is to copy it
	  # directly into the repository.
	  #
	  # This file was written by RCS 5.7, and then the dates were
	  # hacked so that we test year 2000 stuff.  Note also that
	  # "author" names are just strings, as far as importing
	  # RCS files is concerned--they need not correspond to user
	  # IDs on any particular system.
	  #
	  # I also tried writing a file with the RCS supplied with
	  # HPUX A.09.05.  According to "man rcsintro" this is
	  # "Revision Number: 3.0; Release Date: 83/05/11".  There
	  # were a few minor differences like whitespace but at least
	  # in simple cases like this everything else seemed the same
	  # as the file written by RCS 5.7 (so I won't try to make it
	  # a separate test case).

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

	  # OK, here is another one.  This one was written by hand based on
	  # doc/RCSFILES and friends.
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file2,v
head			 	1.5                 ;
     branch        1.2.6;
access ;
symbols;
locks;
testofanewphrase @without newphrase we'd have trouble extending @@ all@ ;
1.5 date 71.01.01.01.00.00; author joe; state bogus; branches; next 1.4;
1.4 date 71.01.01.00.00.05; author joe; state bogus; branches; next 1.3;
1.3 date 70.12.31.15.00.05; author joe; state bogus; branches; next 1.2;
1.2 date 70.12.31.12.15.05; author me; state bogus; branches 1.2.6.1; next 1.1;
1.1 date 70.12.31.11.00.05; author joe; state bogus; branches; next; newph;
1.2.6.1 date 71.01.01.08.00.05; author joe; state Exp; branches; next;
desc @@
1.5 log @@ newphrase1; newphrase2 42; text @head revision@
1.4 log @@ text @d1 1
a1 1
new year revision@
1.3 log @@ text @d1 1
a1 1
old year revision@
1.2 log @@ text @d1 1
a1 1
mid revision@ 1.1

log           @@ text @d1 1
a1 1
start revision@
1.2.6.1 log @@ text @d1 1
a1 1
branch revision@
EOF
	  # First test the default branch.
	  dotest rcs-5 "${testcvs} -q update file2" "U file2"
	  dotest rcs-6 "cat file2" "branch revision"

	  # Now get rid of the default branch, it will get in the way.
	  dotest rcs-7 "${testcvs} admin -b file2" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done"
	  # But we do want to make sure that "cvs admin" leaves the newphrases
	  # in the file.
	  # The extra whitespace regexps are for the RCS library, which does
	  # not preserve whitespace in the dogmatic manner of RCS 5.7. -twp
	  dotest rcs-8 \
"grep testofanewphrase ${CVSROOT_DIRNAME}/first-dir/file2,v" \
"testofanewphrase[	 ][ 	]*@without newphrase we'd have trouble extending @@ all@[	 ]*;"
	  # The easiest way to test for newphrases in deltas and deltatexts
	  # is to just look at the whole file, I guess.
	  dotest rcs-8a "cat ${CVSROOT_DIRNAME}/first-dir/file2,v" \
"head	1\.5;
access;
symbols;
locks;

testofanewphrase	@without newphrase we'd have trouble extending @@ all@;

1\.5
date	71\.01\.01\.01\.00\.00;	author joe;	state bogus;
branches;
next	1\.4;

1\.4
date	71\.01\.01\.00\.00\.05;	author joe;	state bogus;
branches;
next	1\.3;

1\.3
date	70\.12\.31\.15\.00\.05;	author joe;	state bogus;
branches;
next	1\.2;

1\.2
date	70\.12\.31\.12\.15\.05;	author me;	state bogus;
branches
	1\.2\.6\.1;
next	1\.1;

1\.1
date	70\.12\.31\.11\.00\.05;	author joe;	state bogus;
branches;
next	;
newph	;

1\.2\.6\.1
date	71\.01\.01\.08\.00\.05;	author joe;	state Exp;
branches;
next	;


desc
@@


1\.5
log
@@
newphrase1	;
newphrase2	42;
text
@head revision@


1\.4
log
@@
text
@d1 1
a1 1
new year revision@


1\.3
log
@@
text
@d1 1
a1 1
old year revision@


1\.2
log
@@
text
@d1 1
a1 1
mid revision@


1\.1
log
@@
text
@d1 1
a1 1
start revision@


1\.2\.6\.1
log
@@
text
@d1 1
a1 1
branch revision@"

	  # For remote, the "update -p -D" usage seems not to work.
	  # I'm not sure what is going on.
	  if test "x$remote" = "xno"; then

	  if ${testcvs} -q update -p -D '1970-12-31 11:30 UT' file2 \
	      >${TESTDIR}/rcs4.tmp
	  then
	    dotest rcs-9 "cat ${TESTDIR}/rcs4.tmp" "start revision"
	  else
	    fail rcs-9
	  fi

	  if ${testcvs} -q update -p -D '1970-12-31 12:30 UT' file2 \
	      >${TESTDIR}/rcs4.tmp
	  then
	    dotest rcs-10 "cat ${TESTDIR}/rcs4.tmp" "mid revision"
	  else
	    fail rcs-10
	  fi

	  if ${testcvs} -q update -p -D '1971-01-01 00:30 UT' file2 \
	      >${TESTDIR}/rcs4.tmp
	  then
	    dotest rcs-11 "cat ${TESTDIR}/rcs4.tmp" "new year revision"
	  else
	    fail rcs-11
	  fi

	  # Same test as rcs-10, but with am/pm.
	  if ${testcvs} -q update -p -D 'December 31, 1970 12:30pm UT' file2 \
	      >${TESTDIR}/rcs4.tmp
	  then
	    dotest rcs-12 "cat ${TESTDIR}/rcs4.tmp" "mid revision"
	  else
	    fail rcs-12
	  fi

	  # Same test as rcs-11, but with am/pm.
	  if ${testcvs} -q update -p -D 'January 1, 1971 12:30am UT' file2 \
	      >${TESTDIR}/rcs4.tmp
	  then
	    dotest rcs-13 "cat ${TESTDIR}/rcs4.tmp" "new year revision"
	  else
	    fail rcs-13
	  fi

	  fi # end of tests skipped for remote

	  # OK, now make sure cvs log doesn't have any trouble with the
	  # newphrases and such.
	  dotest rcs-14 "${testcvs} -q log file2" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
Working file: file2
head: 1\.5
branch:
locks:
access list:
symbolic names:
keyword substitution: kv
total revisions: 6;	selected revisions: 6
description:
----------------------------
revision 1\.5
date: 1971/01/01 01:00:00;  author: joe;  state: bogus;  lines: ${PLUS}1 -1
\*\*\* empty log message \*\*\*
----------------------------
revision 1\.4
date: 1971/01/01 00:00:05;  author: joe;  state: bogus;  lines: ${PLUS}1 -1
\*\*\* empty log message \*\*\*
----------------------------
revision 1\.3
date: 1970/12/31 15:00:05;  author: joe;  state: bogus;  lines: ${PLUS}1 -1
\*\*\* empty log message \*\*\*
----------------------------
revision 1\.2
date: 1970/12/31 12:15:05;  author: me;  state: bogus;  lines: ${PLUS}1 -1
branches:  1\.2\.6;
\*\*\* empty log message \*\*\*
----------------------------
revision 1\.1
date: 1970/12/31 11:00:05;  author: joe;  state: bogus;
\*\*\* empty log message \*\*\*
----------------------------
revision 1\.2\.6\.1
date: 1971/01/01 08:00:05;  author: joe;  state: Exp;  lines: ${PLUS}1 -1
\*\*\* empty log message \*\*\*
============================================================================="
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest big-3 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..
	  mkdir 2
	  cd 2
	  dotest big-4 "${testcvs} -q get first-dir" "U first-dir/file1"
	  cd ../first-dir
	  echo "add a line to the end" >>file1
	  dotest big-5 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../2/first-dir
	  # The idea here is particularly to test the Rcs-diff response
	  # and the reallocing thereof, for remote.
	  dotest big-6 "${testcvs} -q update" "[UP] file1"
	  cd ../..

	  if test "$keep" = yes; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r first-dir 2
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
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
	  # See many tests (e.g. multibranch) for ordinary sticky tag
	  # operations such as adding files on branches.
	  # See "head" test for interaction between stick tags and HEAD.
	  mkdir 1; cd 1
	  dotest sticky-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest sticky-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir

	  touch file1
	  dotest sticky-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest sticky-14 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  # Now back to tag1
	  dotest sticky-15 "${testcvs} -q update -r tag1" "[UP] file1
${PROG} [a-z]*: file2 is no longer in the repository"

	  rm file1
	  dotest sticky-16 "${testcvs} rm file1" \
"${PROG} [a-z]*: scheduling .file1. for removal
${PROG} [a-z]*: use .${PROG} commit. to remove this file permanently"
	  # Hmm, this command seems to silently remove the tag from
	  # the file.  This appears to be intentional.
	  # The silently part especially strikes me as odd, though.
	  dotest sticky-17 "${testcvs} -q ci -m remove-it" ""
	  dotest sticky-18 "${testcvs} -q update -A" "U file1
U file2"
	  dotest sticky-19 "${testcvs} -q update -r tag1" \
"${PROG} [a-z]*: file1 is no longer in the repository
${PROG} [a-z]*: file2 is no longer in the repository"
	  dotest sticky-20 "${testcvs} -q update -A" "U file1
U file2"

	  # Now try with a numeric revision.
	  dotest sticky-21 "${testcvs} -q update -r 1.1 file1" "U file1"
	  rm file1
	  dotest sticky-22 "${testcvs} rm file1" \
"${PROG} [a-z]*: cannot remove file .file1. which has a numeric sticky tag of .1\.1."
	  # The old behavior was that remove allowed this and then commit
	  # gave an error, which was somewhat hard to clear.  I mean, you
	  # could get into a long elaborate discussion of this being a
	  # conflict and two ways to resolve it, but I don't really see
	  # why CVS should have a concept of conflict that arises, not from
	  # parallel development, but from CVS's own sticky tags.

	  # I'm kind of surprised that the "file1 was lost" doesn't crop
	  # up elsewhere in the testsuite.  It is a long-standing
	  # discrepency between local and remote CVS and should probably
	  # be cleaned up at some point.
	  dotest sticky-23 "${testcvs} -q update -A" \
"${PROG} [a-z]*: warning: file1 was lost
U file1" "U file1"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	keyword)
	  # Test keyword expansion.
	  # Various other tests relate to our ability to correctly
	  # set the keyword expansion mode.
	  # "binfiles" tests "cvs admin -k".
	  # "binfiles" and "binfiles2" test "cvs add -k".
	  # "rdiff" tests "cvs co -k".
	  # "binfiles" (and this test) test "cvs update -k".
	  # "binwrap" tests setting the mode from wrappers.
	  # I don't think any test is testing "cvs import -k".
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
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
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

	  dotest keyword-17 "${testcvs} update -A file1" "U file1"

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

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	keywordlog)
	  # Test the Log keyword.
	  mkdir 1; cd 1
	  dotest keywordlog-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest keywordlog-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir
	  echo change >file1
	  dotest keywordlog-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"

	  # Note that we wanted to try "ci -r 1.3 -m add file1" and CVS
	  # seemed to get all confused, thinking it was adding on a branch
	  # or something.  FIXME?  Do something about this?  Document it
	  # in BUGS or someplace?

	  dotest keywordlog-4 "${testcvs} -q ci -m add file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  cd ../..
	  mkdir 2; cd 2
	  dotest keywordlog-4a "${testcvs} -q co first-dir" "U first-dir/file1"
	  cd ../1/first-dir

	  echo 'xx $''Log$' > file1
	  cat >${TESTDIR}/comment.tmp <<EOF
First log line
Second log line
EOF
	  dotest keywordlog-5 "${testcvs} ci -F ${TESTDIR}/comment.tmp file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  rm -f ${TESTDIR}/comment.tmp
	  dotest keywordlog-6 "${testcvs} -q tag -b br" "T file1"
	  dotest keywordlog-7 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"

	  cd ../../2/first-dir
	  dotest keywordlog-8 "${testcvs} -q update" "[UP] file1"
	  dotest keywordlog-9 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"
	  cd ../../1/first-dir

	  echo "change" >> file1
	  dotest keywordlog-10 "${testcvs} ci -m modify file1" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest keywordlog-11 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.3  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"

	  cd ../../2/first-dir
	  dotest keywordlog-12 "${testcvs} -q update" "[UP] file1"
	  dotest keywordlog-13 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.3  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"

	  cd ../../1/first-dir
	  dotest keywordlog-14 "${testcvs} -q update -r br" "[UP] file1"
	  echo br-change >>file1
	  dotest keywordlog-15 "${testcvs} -q ci -m br-modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"
	  dotest keywordlog-16 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.2\.2\.1  [0-9/]* [0-9:]*  ${username}
xx br-modify
xx
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
br-change"
	  cd ../../2/first-dir
	  dotest keywordlog-17 "${testcvs} -q update -r br" "[UP] file1"
	  dotest keywordlog-18 "cat file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.2\.2\.1  [0-9/]* [0-9:]*  ${username}
xx br-modify
xx
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
br-change"
	  cd ../..
	  dotest keywordlog-19 "${testcvs} -q co -p -r br first-dir/file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.2\.2\.1  [0-9/]* [0-9:]*  ${username}
xx br-modify
xx
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
br-change"
	  dotest keywordlog-20 "${testcvs} -q co -p first-dir/file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.3  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"
	  dotest keywordlog-21 "${testcvs} -q co -p -r 1.2 first-dir/file1" \
"xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.2  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"

	  rm -r 1 2
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
	  mkdir top-dir second-dir
	  dotest toplevel-2 "${testcvs} add top-dir second-dir" \
"Directory ${TESTDIR}/cvsroot/top-dir added to the repository
Directory ${TESTDIR}/cvsroot/second-dir added to the repository"
	  cd top-dir

	  touch file1
	  dotest toplevel-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest toplevel-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/top-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/top-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..

	  cd second-dir
	  touch file2
	  dotest toplevel-3s "${testcvs} add file2" \
"${PROG} [a-z]*: scheduling file .file2. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest toplevel-4s "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/second-dir/file2,v
done
Checking in file2;
${TESTDIR}/cvsroot/second-dir/file2,v  <--  file2
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
	  # There is some sentiment that
	  #   "${PROG} [a-z]*: Updating \.
          #   ${PROG} [a-z]*: Updating top-dir"
	  # is correct but it isn't clear why that would be correct instead
	  # of the remote CVS behavior (which also updates CVSROOT).
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
${PROG} [a-z]*: Updating top-dir"

	  cd ..
	  rm -r 1; mkdir 1; cd 1
	  dotest toplevel-10 "${testcvs} co top-dir" \
"${PROG} [a-z]*: Updating top-dir
U top-dir/file1"
	  # This tests more or less the same thing, in a particularly
	  # "real life" example.
	  dotest toplevel-11 "${testcvs} -q update -d second-dir" \
"U second-dir/file2"

	  # Now remove the CVS directory (people may do this manually,
	  # especially if they formed their habits with CVS
	  # 1.9 and older, which didn't create it.  Or perhaps the working
	  # directory itself was created with 1.9 or older).
	  rm -r CVS
	  # Now set the permissions so we can't recreate it.
	  chmod -w ../1
	  # Now see whether CVS has trouble because it can't create CVS.
	  dotest toplevel-12 "${testcvs} co top-dir" \
"${PROG} [a-z]*: warning: cannot make directory ./CVS: Permission denied
${PROG} [a-z]*: Updating top-dir"
	  chmod +w ../1

	  cd ..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/top-dir
	  ;;

	head)
	  # Testing handling of the HEAD special tag.
	  # There are many cases involving added and removed files
	  # which we don't yet try to deal with.
	  # TODO: We also could be paying much closer attention to
	  # "head of the trunk" versus "head of the default branch".
	  # That is what "cvs import" is doing here (but I didn't really
	  # fully follow through on writing the tests for that case).
	  mkdir imp-dir
	  cd imp-dir
	  echo 'imported contents' >file1
	  # It may seem like we don't do much with file2, but do note that
	  # the "cvs diff" invocations do also diff file2 (and come up empty).
	  echo 'imported contents' >file2
	  dotest head-1 "${testcvs} import -m add first-dir tag1 tag2" \
"N first-dir/file1
N first-dir/file2

No conflicts created by this import"
	  cd ..
	  rm -r imp-dir
	  mkdir 1
	  cd 1
	  dotest head-2 "${testcvs} -q co first-dir" \
"U first-dir/file1
U first-dir/file2"
	  cd first-dir
	  echo 'add a line on trunk' >> file1
	  dotest head-3 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest head-4 "${testcvs} -q tag trunktag" "T file1
T file2"
	  echo 'add a line on trunk after trunktag' >> file1
	  dotest head-5 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest head-6 "${testcvs} -q tag -b br1" "T file1
T file2"
	  dotest head-7 "${testcvs} -q update -r br1" ""
	  echo 'modify on branch' >>file1
	  dotest head-8 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3\.2\.1; previous revision: 1\.3
done"
	  dotest head-9 "${testcvs} -q tag brtag" "T file1
T file2"
	  echo 'modify on branch after brtag' >>file1
	  dotest head-10 "${testcvs} -q ci -m modify" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.3\.2\.2; previous revision: 1\.3\.2\.1
done"
	  # With no sticky tags, HEAD is the head of the trunk.
	  dotest head-trunk-setup "${testcvs} -q update -A" "[UP] file1"
	  dotest head-trunk-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # and diff thinks so too.  Case (a) from the comment in
	  # cvs.texinfo (Common options).
	  dotest_fail head-trunk-diff "${testcvs} -q diff -c -r HEAD -r br1" \
"Index: file1
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.3\.2\.2
diff -c -r1\.3 -r1\.3\.2\.2
\*\*\* file1	[0-9/]* [0-9:]*	1\.3
--- file1	[0-9/]* [0-9:]*	1\.3\.2\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,3 \*\*\*\*
--- 1,5 ----
  imported contents
  add a line on trunk
  add a line on trunk after trunktag
${PLUS} modify on branch
${PLUS} modify on branch after brtag"

	  # With a branch sticky tag, HEAD is the head of the trunk.
	  dotest head-br1-setup "${testcvs} -q update -r br1" "[UP] file1"
	  dotest head-br1-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # But diff thinks that HEAD is "br1".  Case (b) from cvs.texinfo.
	  # Probably people are relying on it.
	  dotest head-br1-diff "${testcvs} -q diff -c -r HEAD -r br1" ""

	  # With a nonbranch sticky tag on a branch,
	  # HEAD is the head of the trunk
	  dotest head-brtag-setup "${testcvs} -q update -r brtag" "[UP] file1"
	  dotest head-brtag-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # But diff thinks that HEAD is "brtag".  Case (c) from
	  # cvs.texinfo (the "strange, maybe accidental" case).
	  dotest_fail head-brtag-diff "${testcvs} -q diff -c -r HEAD -r br1" \
"Index: file1
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.3\.2\.1
retrieving revision 1\.3\.2\.2
diff -c -r1\.3\.2\.1 -r1\.3\.2\.2
\*\*\* file1	[0-9/]* [0-9:]*	1\.3\.2\.1
--- file1	[0-9/]* [0-9:]*	1\.3\.2\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 2,4 \*\*\*\*
--- 2,5 ----
  add a line on trunk
  add a line on trunk after trunktag
  modify on branch
${PLUS} modify on branch after brtag"

	  # With a nonbranch sticky tag on the trunk, HEAD is the head
	  # of the trunk, I think.
	  dotest head-trunktag-setup "${testcvs} -q update -r trunktag" \
"[UP] file1"
	  dotest head-trunktag-check "cat file1" "imported contents
add a line on trunk"
	  dotest head-trunktag-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # Like head-brtag-diff, HEAD is the sticky tag.  Similarly
	  # questionable.
	  dotest_fail head-trunktag-diff \
	    "${testcvs} -q diff -c -r HEAD -r br1" \
"Index: file1
===================================================================
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
retrieving revision 1\.2
retrieving revision 1\.3\.2\.2
diff -c -r1\.2 -r1\.3\.2\.2
\*\*\* file1	[0-9/]* [0-9:]*	1\.2
--- file1	[0-9/]* [0-9:]*	1\.3\.2\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,2 \*\*\*\*
--- 1,5 ----
  imported contents
  add a line on trunk
${PLUS} add a line on trunk after trunktag
${PLUS} modify on branch
${PLUS} modify on branch after brtag"

	  # Also might test what happens if we setup with update -r
	  # HEAD.  In general, if sticky tags matter, does the
	  # behavior of "update -r <foo>" (without -p) depend on the
	  # sticky tags before or after the update?

	  # Note that we are testing both the case where this deletes
	  # a revision (file1) and the case where it does not (file2)
	  dotest_fail head-o0a "${testcvs} admin -o ::br1" \
"${PROG} [a-z]*: Administrating \.
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
${PROG} [a-z]*: cannot remove revision 1\.3\.2\.1 because it has tags
${PROG} [a-z]*: cannot modify RCS file for .file1.
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done"
	  dotest head-o0b "${testcvs} tag -d brtag" \
"${PROG} [a-z]*: Untagging \.
D file1
D file2"
	  dotest head-o1 "${testcvs} admin -o ::br1" \
"${PROG} [a-z]*: Administrating \.
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
deleting revision 1\.3\.2\.1
done
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done"
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	tagdate)
	  # Test combining -r and -D.
	  mkdir 1; cd 1
	  dotest tagdate-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest tagdate-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir

	  echo trunk-1 >file1
	  dotest tagdate-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest tagdate-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest tagdate-5 "${testcvs} -q tag -b br1" "T file1"
	  dotest tagdate-6 "${testcvs} -q tag -b br2" "T file1"
	  echo trunk-2 >file1
	  dotest tagdate-7 "${testcvs} -q ci -m modify-on-trunk" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  # We are testing -r -D where br1 is a (magic) branch without
	  # any revisions.  First the case where br2 doesn't have any
	  # revisions either:
	  dotest tagdate-8 "${testcvs} -q update -p -r br1 -D now" "trunk-1"
	  dotest tagdate-9 "${testcvs} -q update -r br2" "[UP] file1"
	  echo br2-1 >file1
	  dotest tagdate-10 "${testcvs} -q ci -m modify-on-br2" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"
	  # Then the case where br2 does have revisions:
	  dotest tagdate-11 "${testcvs} -q update -p -r br1 -D now" "trunk-1"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	multibranch2)
	  # Commit the first delta on branch A when there is an older
	  # branch, B, that already has a delta.  A and B come from the
	  # same branch point.  Then verify that branches A and B are
	  # in the right order.
	  mkdir 1; cd 1
	  dotest multibranch2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest multibranch2-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
	  cd first-dir

	  echo trunk-1 >file1
	  dotest multibranch2-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest multibranch2-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest multibranch2-5 "${testcvs} -q tag -b A" "T file1"
	  dotest multibranch2-6 "${testcvs} -q tag -b B" "T file1"

	  dotest multibranch2-7 "${testcvs} -q update -r B" ''
	  echo branch-B >file1
	  dotest multibranch2-8 "${testcvs} -q ci -m modify-on-B" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"

	  dotest multibranch2-9 "${testcvs} -q update -r A" '[UP] file1'
	  echo branch-A >file1
	  # When using cvs-1.9.20, this commit gets a failed assertion in rcs.c.
	  dotest multibranch2-10 "${testcvs} -q ci -m modify-on-A" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  dotest multibranch2-11 "${testcvs} -q log" \
"
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	B: 1\.1\.0\.4
	A: 1\.1\.0\.2
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
branches:  1\.1\.2;  1\.1\.4;
add
----------------------------
revision 1\.1\.4\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;  lines: ${PLUS}1 -1
modify-on-B
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;  lines: ${PLUS}1 -1
modify-on-A
============================================================================="

	  # This one is more concise.
	  dotest multibranch2-12 "${testcvs} -q log -r1.1" \
"
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	B: 1\.1\.0\.4
	A: 1\.1\.0\.2
keyword substitution: kv
total revisions: 3;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
branches:  1\.1\.2;  1\.1\.4;
add
============================================================================="

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;


	admin)
	  # More "cvs admin" tests.
	  # The basicb-21 test tests rejecting an illegal option.
	  # For -l and -u, see "reserved" and "keyword" tests.
	  # "binfiles" test has a test of "cvs admin -k".
	  # "log2" test has tests of -t and -q options to cvs admin.
	  # "rcs" tests -b option also.
	  # For -o, see:
	  #   admin-22-o1 through admin-23 (various cases not involving ::)
	  #   binfiles2-o* (:rev, rev on trunk; rev:, deleting entire branch)
	  #   basica-o1 through basica-o3 (basic :: usage)
	  #   head-o1 (::branch, where this deletes a revision or is noop)
	  #   branches-o1 (::branch, similar, with different branch topology)
	  #   log-o1 (1.3.2.1::)
	  #   binfiles-o1 (1.3:: and ::1.3)
	  #   Also could be testing:
	  #     1.3.2.6::1.3.2.8
	  #     1.3.2.6::1.3.2
	  #     1.3.2.1::1.3.2.6
	  #     1.3::1.3.2.6 (error?  or synonym for ::1.3.2.6?)

	  mkdir 1; cd 1
	  dotest admin-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest admin-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
          cd first-dir

	  dotest_fail admin-3 "${testcvs} -q admin -i file1" \
"${PROG} admin: the -i option to admin is not supported
${PROG} admin: run add or import to create an RCS file
${PROG} \[admin aborted\]: specify ${PROG} -H admin for usage information"
	  dotest_fail admin-4 "${testcvs} -q log file1" \
"${PROG} [a-z]*: nothing known about file1"

	  # Set up some files, file2 a plain one and file1 with a revision
	  # on a branch.
	  touch file1 file2
	  dotest admin-5 "${testcvs} add file1 file2" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: scheduling file .file2. for addition
${PROG} [a-z]*: use .${PROG} commit. to add these files permanently"
	  dotest admin-6 "${testcvs} -q ci -m add" \
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
	  dotest admin-7 "${testcvs} -q tag -b br" "T file1
T file2"
	  dotest admin-8 "${testcvs} -q update -r br" ""
	  echo 'add a line on the branch' >> file1
	  dotest admin-9 "${testcvs} -q ci -m modify-on-branch" \
"Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest admin-10 "${testcvs} -q update -A" "U file1"

	  # Try to recurse with a numeric revision arg.
	  # If we wanted to comprehensive about this, we would also test
	  # this for -l, -u, and all the different -o syntaxes.
	  dotest_fail admin-10a "${testcvs} -q admin -b1.1.2" \
"${PROG} [a-z]*: while processing more than one file:
${PROG} \[[a-z]* aborted\]: attempt to specify a numeric revision"
	  dotest_fail admin-10b "${testcvs} -q admin -m1.1:bogus file1 file2" \
"${PROG} [a-z]*: while processing more than one file:
${PROG} \[[a-z]* aborted\]: attempt to specify a numeric revision"

	  # Note that -s option applies to the new default branch, not
	  # the old one.
	  # Also note that the implementation of -a via "rcs" requires
	  # no space between -a and the argument.  However, we expect
	  # to change that once CVS parses options.
	  dotest admin-11 "${testcvs} -q admin -afoo,bar -abaz \
-b1.1.2 -cxx -U -sfoo file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"

	  dotest admin-12 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch: 1\.1\.2
locks:
access list:
	foo
	bar
	baz
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;
add
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: foo;  lines: ${PLUS}1 -0
modify-on-branch
============================================================================="

	  # "cvs log" doesn't print the comment leader.  RCS 5.7 will print
	  # the comment leader only if one specifies "-V4" to rlog.  So it
	  # seems like the only way to test it is by looking at the RCS file
	  # directly.  This also serves as a test of exporting RCS files
	  # (analogous to the import tests in "rcs").
	  # Rather than try to write a rigorous check for whether the
	  # file CVS exports is legal, we just write a simpler
	  # test for what CVS actually exports, and figure we can revise
	  # the check as needed (within the confines of the RCS5 format as
	  # documented in RCSFILES).
	  dotest admin-13 "cat ${CVSROOT_DIRNAME}/first-dir/file1,v" \
"head	1\.1;
branch	1\.1\.2;
access
	foo
	bar
	baz;
symbols
	br:1\.1\.0\.2;
locks;
comment	@xx@;


1\.1
date	[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches
	1\.1\.2\.1;
next	;

1\.1\.2\.1
date	[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state foo;
branches;
next	;


desc
@@


1\.1
log
@add
@
text
@@


1\.1\.2\.1
log
@modify-on-branch
@
text
@a0 1
add a line on the branch
@"
	  dotest admin-14 "${testcvs} -q admin -aauth3 -aauth2,foo \
-soneone:1.1 -m1.1:changed-log-message -ntagone: file2" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
done"
	  dotest admin-15 "${testcvs} -q log file2" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
Working file: file2
head: 1\.1
branch:
locks: strict
access list:
	auth3
	auth2
	foo
symbolic names:
	tagone: 1\.1
	br: 1\.1\.0\.2
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: oneone;
changed-log-message
============================================================================="

	  dotest admin-16 "${testcvs} -q admin \
-A${CVSROOT_DIRNAME}/first-dir/file2,v -b -L -Nbr:1.1 file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest admin-17 "${testcvs} -q log file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
	foo
	bar
	baz
	auth3
	auth2
symbolic names:
	br: 1\.1
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;
add
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: foo;  lines: ${PLUS}1 -0
modify-on-branch
============================================================================="

	  dotest_fail admin-18 "${testcvs} -q admin -nbr:1.1.2 file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
${PROG} [a-z]*: ${TESTDIR}/cvsroot/first-dir/file1,v: symbolic name br already bound to 1\.1
${PROG} [a-z]*: cannot modify RCS file for .file1."
	  dotest admin-19 "${testcvs} -q admin -ebaz -ebar,auth3 -nbr file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest admin-20 "${testcvs} -q log file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
	foo
	auth2
symbolic names:
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.2;
add
----------------------------
revision 1.1.2.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: foo;  lines: ${PLUS}1 -0
modify-on-branch
============================================================================="

	  # OK, this is starting to get ridiculous, in terms of
	  # testing a feature (access lists) which doesn't do anything
	  # useful, but what about nonexistent files and
	  # relative pathnames in admin -A?
	  dotest_fail admin-19a-nonexist \
"${testcvs} -q admin -A${TESTDIR}/foo/bar file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
${PROG} [a-z]*: Couldn't open rcs file .${TESTDIR}/foo/bar.: No such file or directory
${PROG} \[[a-z]* aborted\]: cannot continue"

	  # In the remote case, we are cd'd off into the temp directory
	  # and so these tests give "No such file or directory" errors.
	  if test "x$remote" = xno; then

	  dotest admin-19a-admin "${testcvs} -q admin -A../../cvsroot/first-dir/file2,v file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  dotest admin-19a-log "${testcvs} -q log -h -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
	foo
	auth2
	auth3
keyword substitution: kv
total revisions: 2
============================================================================="
	  # Put the access list back, to avoid special cases later.
	  dotest admin-19a-fix "${testcvs} -q admin -eauth3 file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done"
	  fi # end of tests skipped for remote

	  # Add another revision to file2, so we can delete one.
	  echo 'add a line' >> file2
	  dotest admin-21 "${testcvs} -q ci -m modify file2" \
"Checking in file2;
${TESTDIR}/cvsroot/first-dir/file2,v  <--  file2
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest admin-22 "${testcvs} -q admin -o1.1 file2" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
deleting revision 1\.1
done"
	  # Test admin -o.  More variants that we could be testing:
	  # * REV: [on branch]
	  # * REV1:REV2 [deleting whole branch]
	  # * high branch numbers (e.g. 1.2.2.3.2.3)
	  # ... and probably others.  See RCS_delete_revs for ideas.

	  echo first rev > aaa
	  dotest admin-22-o1 "${testcvs} add aaa" \
"${PROG} [a-z]*: scheduling file .aaa. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest admin-22-o2 "${testcvs} -q ci -m first aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
done
Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
initial revision: 1\.1
done"
	  echo second rev >> aaa
	  dotest admin-22-o3 "${testcvs} -q ci -m second aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.2; previous revision: 1\.1
done"
	  echo third rev >> aaa
	  dotest admin-22-o4 "${testcvs} -q ci -m third aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.3; previous revision: 1\.2
done"
	  echo fourth rev >> aaa
	  dotest admin-22-o5 "${testcvs} -q ci -m fourth aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.4; previous revision: 1\.3
done"
	  echo fifth rev >>aaa
	  dotest admin-22-o6 "${testcvs} -q ci -m fifth aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.5; previous revision: 1\.4
done"
	  echo sixth rev >> aaa
	  dotest admin-22-o7 "${testcvs} -q ci -m sixth aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.6; previous revision: 1\.5
done"
	  dotest admin-22-o8 "${testcvs} admin -l1.6 aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
1\.6 locked
done"
	  dotest admin-22-o9 "${testcvs} log -r1.6 aaa" "
RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
Working file: aaa
head: 1\.6
branch:
locks: strict
	${username}: 1\.6
access list:
symbolic names:
keyword substitution: kv
total revisions: 6;	selected revisions: 1
description:
----------------------------
revision 1\.6	locked by: ${username};
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
sixth
============================================================================="
	  dotest_fail admin-22-o10 "${testcvs} admin -o1.5: aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
${PROG} [a-z]*: ${TESTDIR}/cvsroot/first-dir/aaa,v: can't remove locked revision 1\.6
${PROG} [a-z]*: cannot modify RCS file for .aaa."
	  dotest admin-22-o11 "${testcvs} admin -u aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
1\.6 unlocked
done"
	  dotest admin-22-o12 "${testcvs} admin -o1.5: aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
deleting revision 1\.6
deleting revision 1\.5
done"
	  dotest admin-22-o13 "${testcvs} log aaa" "
RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
Working file: aaa
head: 1\.4
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 4;	selected revisions: 4
description:
----------------------------
revision 1\.4
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
fourth
----------------------------
revision 1\.3
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
third
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
second
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
first
============================================================================="

	  dotest admin-22-o14 "${testcvs} tag -b -r1.3 br1 aaa" "T aaa"
	  dotest admin-22-o15 "${testcvs} update -rbr1 aaa" "U aaa"
	  echo new branch rev >> aaa
	  dotest admin-22-o16 "${testcvs} ci -m new-branch aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.1; previous revision: 1\.3
done"
	  dotest_fail admin-22-o17 "${testcvs} admin -o1.2:1.4 aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
deleting revision 1\.4
${PROG} [a-z]*: ${TESTDIR}/cvsroot/first-dir/aaa,v: can't remove branch point 1\.3
${PROG} [a-z]*: cannot modify RCS file for .aaa."
	  dotest admin-22-o18 "${testcvs} update -p -r1.4 aaa" \
"===================================================================
Checking out aaa
RCS:  ${TESTDIR}/cvsroot/first-dir/aaa,v
VERS: 1\.4
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
first rev
second rev
third rev
fourth rev"
	  echo second branch rev >> aaa
	  dotest admin-22-o19 "${testcvs} ci -m branch-two aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.2; previous revision: 1\.3\.2\.1
done"
	  echo third branch rev >> aaa
	  dotest admin-22-o20 "${testcvs} ci -m branch-three aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.3; previous revision: 1\.3\.2\.2
done"
	  echo fourth branch rev >> aaa
	  dotest admin-22-o21 "${testcvs} ci -m branch-four aaa" \
"Checking in aaa;
${TESTDIR}/cvsroot/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.4; previous revision: 1\.3\.2\.3
done"
	  dotest admin-22-o22 "${testcvs} admin -o:1.3.2.3 aaa" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
deleting revision 1\.3\.2\.1
deleting revision 1\.3\.2\.2
deleting revision 1\.3\.2\.3
done"
	  dotest admin-22-o23 "${testcvs} log aaa" "
RCS file: ${TESTDIR}/cvsroot/first-dir/aaa,v
Working file: aaa
head: 1\.4
branch:
locks: strict
access list:
symbolic names:
	br1: 1\.3\.0\.2
keyword substitution: kv
total revisions: 5;	selected revisions: 5
description:
----------------------------
revision 1\.4
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
fourth
----------------------------
revision 1\.3
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
branches:  1\.3\.2;
third
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
second
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
first
----------------------------
revision 1\.3\.2\.4
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}4 -0
branch-four
============================================================================="

	  dotest admin-22-o24 "${testcvs} -q update -p -r 1.3.2.4 aaa" \
"first rev
second rev
third rev
new branch rev
second branch rev
third branch rev
fourth branch rev"

	  # The bit here about how there is a "tagone" tag pointing to
	  # a nonexistent revision is documented by rcs.  I dunno, I
	  # wonder whether the "cvs admin -o" should give a warning in
	  # this case.
	  dotest admin-23 "${testcvs} -q log file2" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file2,v
Working file: file2
head: 1\.2
branch:
locks: strict
access list:
	auth3
	auth2
	foo
symbolic names:
	tagone: 1\.1
	br: 1\.1\.0\.2
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
modify
============================================================================="

	  dotest admin-25 "cat ${CVSROOT_DIRNAME}/first-dir/file1,v" \
"head	1\.1;
access
	foo
	auth2;
symbols;
locks; strict;
comment	@xx@;


1\.1
date	[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches
	1\.1\.2\.1;
next	;

1\.1\.2\.1
date	[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state foo;
branches;
next	;


desc
@@


1\.1
log
@add
@
text
@@


1\.1\.2\.1
log
@modify-on-branch
@
text
@a0 1
add a line on the branch
@"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	reserved)
	  # Tests of reserved checkouts.  Eventually this will test
	  # rcslock.pl (or equivalent) and all kinds of stuff.  Right
	  # now it just does some very basic checks on cvs admin -u
	  # and cvs admin -l.
	  # Also should test locking on a branch (and making sure that
	  # locks from one branch don't get mixed up with those from
	  # another.  Both the case where one of the branches is the
	  # main branch, and in which neither one is).
	  # See also test keyword, which tests that keywords and -kkvl
	  # do the right thing in the presence of locks.

	  # The usual setup, directory first-dir containing file file1.
	  mkdir 1; cd 1
	  dotest reserved-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest reserved-2 "${testcvs} add first-dir" \
"Directory ${TESTDIR}/cvsroot/first-dir added to the repository"
          cd first-dir
	  touch file1
	  dotest reserved-3 "${testcvs} add file1" \
"${PROG} [a-z]*: scheduling file .file1. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest reserved-4 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
done
Checking in file1;
${TESTDIR}/cvsroot/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  dotest reserved-5 "${testcvs} -q admin -l file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
1\.1 locked
done"
	  dotest reserved-6 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
	${username}: 1\.1
access list:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1	locked by: ${username};
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add
============================================================================="

	  # Note that this just tests the owner of the lock giving
	  # it up.  It doesn't test breaking a lock.
	  dotest reserved-7 "${testcvs} -q admin -u file1" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
1\.1 unlocked
done"

	  dotest reserved-8 "${testcvs} log -N file1" "
RCS file: ${TESTDIR}/cvsroot/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add
============================================================================="

	  # rcslock.pl tests.  Of course, the point isn't to test
	  # rcslock.pl from the distribution but equivalent
	  # functionality (for example, many sites may have an old
	  # rcslock.pl).  The functionality of this hook falls
	  # short of the real rcslock.pl though.
	  # Note that we can use rlog or look at the RCS file directly,
	  # but we can't use "cvs log" because "cvs commit" has a lock.

	  cat >${TESTDIR}/lockme <<EOF
#!${TESTSHELL}
line=\`grep <\$1/\$2,v 'locks ${username}:1\.[0-9];'\`
if test -z "\$line"; then
  # It isn't locked
  exit 0
else
  user=\`echo \$line | sed -e 's/locks \\(${username}\\):[0-9.]*;.*/\\1/'\`
  version=\`echo \$line | sed -e 's/locks ${username}:\\([0-9.]*\\);.*/\\1/'\`
  echo "\$user has file a-lock locked for version  \$version"
  exit 1
fi
EOF
	  chmod +x ${TESTDIR}/lockme

	  echo stuff > a-lock
	  dotest reserved-9 "${testcvs} add a-lock" \
"${PROG} [a-z]*: scheduling file .a-lock. for addition
${PROG} [a-z]*: use .${PROG} commit. to add this file permanently"
	  dotest reserved-10 "${testcvs} -q ci -m new a-lock" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/a-lock,v
done
Checking in a-lock;
${TESTDIR}/cvsroot/first-dir/a-lock,v  <--  a-lock
initial revision: 1\.1
done"
	  # FIXME: the contents of CVSROOT fluctuate a lot
	  # here. Maybe the expect pattern should just
	  # confirm that commitinfo is one of the files checked out,
	  # but for now we just check that CVS exited with success.
	  cd ..
	  if ${testcvs} -q co CVSROOT >>${LOGFILE} ; then
	    pass reserved-11
	  else
	    fail reserved-11
	  fi
	  cd CVSROOT
	  echo "DEFAULT ${TESTDIR}/lockme" >>commitinfo
	  dotest reserved-12 "${testcvs} -q ci -m rcslock commitinfo" \
"Checking in commitinfo;
${TESTDIR}/cvsroot/CVSROOT/commitinfo,v  <--  commitinfo
new revision: 1\.2; previous revision: 1\.1
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..; cd first-dir

	  # Simulate (approximately) what a-lock would look like
	  # if someone else had locked revision 1.1.
	  sed -e 's/locks; strict;/locks fred:1.1; strict;/' ${TESTDIR}/cvsroot/first-dir/a-lock,v > a-lock,v
	  chmod 644 ${TESTDIR}/cvsroot/first-dir/a-lock,v
	  dotest reserved-13 "mv a-lock,v ${TESTDIR}/cvsroot/first-dir/a-lock,v"
	  chmod 444 ${TESTDIR}/cvsroot/first-dir/a-lock,v
	  echo more stuff >> a-lock
	  dotest_fail reserved-13b "${testcvs} ci -m '' a-lock" \
"fred has file a-lock locked for version  1\.1
${PROG} [a-z]*: Pre-commit check failed
${PROG} \[[a-z]* aborted\]: correct above errors first!"

	  dotest reserved-14 "${testcvs} admin -u1.1 a-lock" \
"RCS file: ${TESTDIR}/cvsroot/first-dir/a-lock,v
1\.1 unlocked
done"
	  dotest reserved-15 "${testcvs} -q ci -m success a-lock" \
"Checking in a-lock;
${TESTDIR}/cvsroot/first-dir/a-lock,v  <--  a-lock
new revision: 1\.2; previous revision: 1\.1
done"

	  # undo commitinfo changes
	  cd ../CVSROOT
	  echo '# vanilla commitinfo' >commitinfo
	  dotest reserved-16 "${testcvs} -q ci -m back commitinfo" \
"Checking in commitinfo;
${TESTDIR}/cvsroot/CVSROOT/commitinfo,v  <--  commitinfo
new revision: 1\.3; previous revision: 1\.2
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  cd ..; rm -r CVSROOT; cd first-dir

	  cd ../..
	  rm -r 1
	  rm ${TESTDIR}/lockme
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	cvsadm)
	  # These test check the content of CVS' administrative
	  # files as they are checked out in various configurations.
	  # (As a side note, I'm not using the "-q" flag in any of
	  # this code, which should provide some extra checking for
          # those messages which don't seem to be checked thoroughly
	  # anywhere else.)  To do a thorough test, we need to make
	  # a bunch of modules in various configurations.
	  #
	  # <1mod> is a directory at the top level of cvsroot
	  #    ``foo bar''
	  # <2mod> is a directory at the second level of cvsroot
	  #    ``foo bar/baz''
	  # <1d1mod> is a directory at the top level which is
	  #   checked out into another directory
	  #     ``foo -d bar baz''
	  # <1d2mod> is a directory at the second level which is
	  #   checked out into another directory
	  #     ``foo -d bar baz/quux''
	  # <2d1mod> is a directory at the top level which is
	  #   checked out into a directory that is two deep
	  #     ``foo -d bar/baz quux''
	  # <2d2mod> is a directory at the second level which is
	  #   checked out into a directory that is two deep
	  #     ``foo -d bar/baz quux''
	  #
	  # The tests do each of these types separately and in twos.
	  # We also repeat each test -d flag for 1-deep and 2-deep
	  # directories.
	  #
	  # Each test should check the output for the Repository
	  # file, since that is the one which varies depending on 
	  # the directory and how it was checked out.
	  #
	  # Yes, this is verbose, but at least it's very thorough.

	  # convenience variables
	  REP=${CVSROOT}

	  # First, check out the modules file and edit it.
	  mkdir 1; cd 1
	  dotest cvsadm-1 "${testcvs} co CVSROOT/modules" \
"U CVSROOT/modules"

	  # Try to determine whether RELATIVE_REPOS is defined
	  # so that we can make the following a lot less
	  # verbose.

	  echo "${CVSROOT_DIRNAME}/." > ${TESTDIR}/dotest.abs
	  echo "." > ${TESTDIR}/dotest.rel
	  if cmp ${TESTDIR}/dotest.abs CVS/Repository >/dev/null 2>&1; then
	    AREP="${CVSROOT_DIRNAME}/"
	  elif cmp ${TESTDIR}/dotest.rel CVS/Repository >/dev/null 2>&1; then
	    AREP=""
	  else
	    fail "Cannot figure out if RELATIVE_REPOS is defined."
	  fi

	  # Test CVS/Root once.  Since there is only one part of
	  # the code which writes CVS/Root files (Create_Admin),
	  # there is no point in testing this every time.
	  dotest cvsadm-1a "cat CVS/Root" ${REP}
	  dotest cvsadm-1b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1c "cat CVSROOT/CVS/Root" ${REP}
	  dotest cvsadm-1d "cat CVSROOT/CVS/Repository" \
"${AREP}CVSROOT"
          # All of the defined module names begin with a number.
	  # All of the top-level directory names begin with "dir".
	  # All of the subdirectory names begin with "sub".
	  # All of the top-level modules begin with "mod".
	  echo "# Module defs for cvsadm tests" > CVSROOT/modules
	  echo "1mod mod1" >> CVSROOT/modules
	  echo "1mod-2 mod1-2" >> CVSROOT/modules
	  echo "2mod mod2/sub2" >> CVSROOT/modules
	  echo "2mod-2 mod2-2/sub2-2" >> CVSROOT/modules
	  echo "1d1mod -d dir1d1 mod1" >> CVSROOT/modules
	  echo "1d1mod-2 -d dir1d1-2 mod1-2" >> CVSROOT/modules
	  echo "1d2mod -d dir1d2 mod2/sub2" >> CVSROOT/modules
	  echo "1d2mod-2 -d dir1d2-2 mod2-2/sub2-2" >> CVSROOT/modules
	  echo "2d1mod -d dir2d1/sub2d1 mod1" >> CVSROOT/modules
	  echo "2d1mod-2 -d dir2d1-2/sub2d1-2 mod1-2" >> CVSROOT/modules
	  echo "2d2mod -d dir2d2/sub2d2 mod2/sub2" >> CVSROOT/modules
	  echo "2d2mod-2 -d dir2d2-2/sub2d2-2 mod2-2/sub2-2" >> CVSROOT/modules
	  dotest cvsadm-1e "${testcvs} ci -m add-modules" \
"${PROG} [a-z]*: Examining .
${PROG} [a-z]*: Examining CVSROOT
Checking in CVSROOT/modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} [a-z]*: Rebuilding administrative file database"
	  rm -rf CVS CVSROOT;

	  # Create the various modules
	  mkdir ${CVSROOT_DIRNAME}/mod1
	  mkdir ${CVSROOT_DIRNAME}/mod1-2
	  mkdir ${CVSROOT_DIRNAME}/mod2
	  mkdir ${CVSROOT_DIRNAME}/mod2/sub2
	  mkdir ${CVSROOT_DIRNAME}/mod2-2
	  mkdir ${CVSROOT_DIRNAME}/mod2-2/sub2-2
	  dotest cvsadm-2 "${testcvs} co mod1 mod1-2 mod2 mod2-2" \
"${PROG} [a-z]*: Updating mod1
${PROG} [a-z]*: Updating mod1-2
${PROG} [a-z]*: Updating mod2
${PROG} [a-z]*: Updating mod2/sub2
${PROG} [a-z]*: Updating mod2-2
${PROG} [a-z]*: Updating mod2-2/sub2-2"

	  # Populate the directories for the halibut
	  echo "file1" > mod1/file1
	  echo "file1-2" > mod1-2/file1-2
	  echo "file2" > mod2/sub2/file2
	  echo "file2-2" > mod2-2/sub2-2/file2-2
	  dotest cvsadm-2a "${testcvs} add mod1/file1 mod1-2/file1-2 mod2/sub2/file2 mod2-2/sub2-2/file2-2" \
"${PROG} [a-z]*: scheduling file .mod1/file1. for addition
${PROG} [a-z]*: scheduling file .mod1-2/file1-2. for addition
${PROG} [a-z]*: scheduling file .mod2/sub2/file2. for addition
${PROG} [a-z]*: scheduling file .mod2-2/sub2-2/file2-2. for addition
${PROG} [a-z]*: use '${PROG} commit' to add these files permanently"

	  dotest cvsadm-2b "${testcvs} ci -m yup mod1 mod1-2 mod2 mod2-2" \
"${PROG} [a-z]*: Examining mod1
${PROG} [a-z]*: Examining mod1-2
${PROG} [a-z]*: Examining mod2
${PROG} [a-z]*: Examining mod2/sub2
${PROG} [a-z]*: Examining mod2-2
${PROG} [a-z]*: Examining mod2-2/sub2-2
RCS file: ${CVSROOT_DIRNAME}/mod1/file1,v
done
Checking in mod1/file1;
${CVSROOT_DIRNAME}/mod1/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/mod1-2/file1-2,v
done
Checking in mod1-2/file1-2;
${CVSROOT_DIRNAME}/mod1-2/file1-2,v  <--  file1-2
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/mod2/sub2/file2,v
done
Checking in mod2/sub2/file2;
${CVSROOT_DIRNAME}/mod2/sub2/file2,v  <--  file2
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/mod2-2/sub2-2/file2-2,v
done
Checking in mod2-2/sub2-2/file2-2;
${CVSROOT_DIRNAME}/mod2-2/sub2-2/file2-2,v  <--  file2-2
initial revision: 1.1
done"
	  # Finished creating the modules -- clean up.
	  rm -rf CVS mod1 mod1-2 mod2 mod2-2
	  # Done.

	  ##################################################
	  ## Start the dizzying array of possibilities.
	  ## Begin with each module type separately.
	  ##################################################
	  
	  # Pattern -- after each checkout, first check the top-level
	  # CVS directory.  Then, check the directories in numerical
	  # order.

	  dotest cvsadm-3 "${testcvs} co 1mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1"
	  dotest cvsadm-3b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-3d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS 1mod

	  dotest cvsadm-4 "${testcvs} co 2mod" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2"
	  dotest cvsadm-4b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-4d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 2mod

	  dotest cvsadm-5 "${testcvs} co 1d1mod" \
"${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1"
	  dotest cvsadm-5b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-5d "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir1d1

	  dotest cvsadm-6 "${testcvs} co 1d2mod" \
"${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2"
	  dotest cvsadm-6b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-6d "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir1d2

	  dotest cvsadm-7 "${testcvs} co 2d1mod" \
"${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  dotest cvsadm-7b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-7d "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-7f "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir2d1

	  dotest cvsadm-8 "${testcvs} co 2d2mod" \
"${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  dotest cvsadm-8b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-8d "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-8f "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir2d2

	  ##################################################
	  ## You are in a shell script of twisted little
	  ## module combination statements, all alike.
	  ##################################################

	  ### 1mod
	  
	  dotest cvsadm-9 "${testcvs} co 1mod 1mod-2" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1
${PROG} [a-z]*: Updating 1mod-2
U 1mod-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-9b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-9d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1mod copy
	  dotest cvsadm-9f "cat 1mod-2/CVS/Repository" \
"${AREP}mod1-2"
	  rm -rf CVS 1mod 1mod-2

	  # 1mod 2mod redmod bluemod
	  dotest cvsadm-10 "${testcvs} co 1mod 2mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1
${PROG} [a-z]*: Updating 2mod
U 2mod/file2"
	  # the usual for the top level
	  dotest cvsadm-10b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-10d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2dmod
	  dotest cvsadm-10f "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 1mod 2mod

	  dotest cvsadm-11 "${testcvs} co 1mod 1d1mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1
${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1"
	  # the usual for the top level
	  dotest cvsadm-11b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-11d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d1mod
	  dotest cvsadm-11f "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS 1mod dir1d1

	  dotest cvsadm-12 "${testcvs} co 1mod 1d2mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1
${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2"
	  # the usual for the top level
	  dotest cvsadm-12b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-12d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-12f "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 1mod dir1d2

	  dotest cvsadm-13 "${testcvs} co 1mod 2d1mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1
${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-13b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-13d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-13f "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-13h "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS 1mod dir2d1

	  dotest cvsadm-14 "${testcvs} co 1mod 2d2mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1
${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-14b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-14d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-14f "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-14h "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 1mod dir2d2


	  ### 2mod
	  
	  dotest cvsadm-15 "${testcvs} co 2mod 2mod-2" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2
${PROG} [a-z]*: Updating 2mod-2
U 2mod-2/file2-2"
	  # the usual for the top level
	  dotest cvsadm-15b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-15d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2mod copy
	  dotest cvsadm-15f "cat 2mod-2/CVS/Repository" \
"${AREP}mod2-2/sub2-2"
	  rm -rf CVS 2mod 2mod-2


	  dotest cvsadm-16 "${testcvs} co 2mod 1d1mod" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2
${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1"
	  # the usual for the top level
	  dotest cvsadm-16b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-16d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 1d1mod
	  dotest cvsadm-16f "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS 2mod dir1d1

	  dotest cvsadm-17 "${testcvs} co 2mod 1d2mod" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2
${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2"
	  # the usual for the top level
	  dotest cvsadm-17b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-17d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 1d2mod
	  dotest cvsadm-17f "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 2mod dir1d2

	  dotest cvsadm-18 "${testcvs} co 2mod 2d1mod" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2
${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-18b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-18d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-18f "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-18h "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS 2mod dir2d1

	  dotest cvsadm-19 "${testcvs} co 2mod 2d2mod" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2
${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-19b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-19d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-19f "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-19h "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 2mod dir2d2


	  ### 1d1mod

	  dotest cvsadm-20 "${testcvs} co 1d1mod 1d1mod-2" \
"${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1
${PROG} [a-z]*: Updating dir1d1-2
U dir1d1-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-20b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-20d "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d1mod copy
	  dotest cvsadm-20f "cat dir1d1-2/CVS/Repository" \
"${AREP}mod1-2"
	  rm -rf CVS dir1d1 dir1d1-2

	  dotest cvsadm-21 "${testcvs} co 1d1mod 1d2mod" \
"${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1
${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2"
	  # the usual for the top level
	  dotest cvsadm-21b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-21d "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-21f "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir1d1 dir1d2

	  dotest cvsadm-22 "${testcvs} co 1d1mod 2d1mod" \
"${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1
${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-22b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-22d "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-22f "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-22h "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir1d1 dir2d1

	  dotest cvsadm-23 "${testcvs} co 1d1mod 2d2mod" \
"${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1
${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-23b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-23d "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-23f "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-23h "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir1d1 dir2d2


	  ### 1d2mod

	  dotest cvsadm-24 "${testcvs} co 1d2mod 1d2mod-2" \
"${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2
${PROG} [a-z]*: Updating dir1d2-2
U dir1d2-2/file2-2"
	  # the usual for the top level
	  dotest cvsadm-24b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d2mod
	  dotest cvsadm-24d "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 1d2mod copy
	  dotest cvsadm-24f "cat dir1d2-2/CVS/Repository" \
"${AREP}mod2-2/sub2-2"
	  rm -rf CVS dir1d2 dir1d2-2

	  dotest cvsadm-25 "${testcvs} co 1d2mod 2d1mod" \
"${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2
${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-25b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d2mod
	  dotest cvsadm-25d "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-25f "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-25h "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir1d2 dir2d1

	  dotest cvsadm-26 "${testcvs} co 1d2mod 2d2mod" \
"${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2
${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-26b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 1d2mod
	  dotest cvsadm-26d "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-26f "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-26h "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir1d2 dir2d2


	  # 2d1mod

	  dotest cvsadm-27 "${testcvs} co 2d1mod 2d1mod-2" \
"${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1
${PROG} [a-z]*: Updating dir2d1-2/sub2d1-2
U dir2d1-2/sub2d1-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-27b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2d1mod
	  dotest cvsadm-27d "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-27f "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-27h "cat dir2d1-2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-27j "cat dir2d1-2/sub2d1-2/CVS/Repository" \
"${AREP}mod1-2"
	  rm -rf CVS dir2d1 dir2d1-2

	  dotest cvsadm-28 "${testcvs} co 2d1mod 2d2mod" \
"${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1
${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-28b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2d1mod
	  dotest cvsadm-28d "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-28f "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-28h "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-28j "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir2d1 dir2d2

	  
	  # 2d2mod

	  dotest cvsadm-29 "${testcvs} co 2d2mod 2d2mod-2" \
"${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2
${PROG} [a-z]*: Updating dir2d2-2/sub2d2-2
U dir2d2-2/sub2d2-2/file2-2"
	  # the usual for the top level
	  dotest cvsadm-29b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for 2d2mod
	  dotest cvsadm-29d "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-29f "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-29h "cat dir2d2-2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-29j "cat dir2d2-2/sub2d2-2/CVS/Repository" \
"${AREP}mod2-2/sub2-2"
	  rm -rf CVS dir2d2 dir2d2-2

	  ##################################################
	  ## And now, all of that again using the "-d" flag
	  ## on the command line.
	  ##################################################

	  dotest cvsadm-1d3 "${testcvs} co -d dir 1mod" \
"${PROG} [a-z]*: Updating dir
U dir/file1"
	  dotest cvsadm-1d3b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1d3d "cat dir/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d4 "${testcvs} co -d dir 2mod" \
"${PROG} [a-z]*: Updating dir
U dir/file2"
	  dotest cvsadm-1d4b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1d4d "cat dir/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d5 "${testcvs} co -d dir 1d1mod" \
"${PROG} [a-z]*: Updating dir
U dir/file1"
	  dotest cvsadm-1d5b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1d5d "cat dir/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d6 "${testcvs} co -d dir 1d2mod" \
"${PROG} [a-z]*: Updating dir
U dir/file2"
	  dotest cvsadm-1d6b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1d6d "cat dir/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d7 "${testcvs} co -d dir 2d1mod" \
"${PROG} [a-z]*: Updating dir
U dir/file1"
	  dotest cvsadm-1d7b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1d7d "cat dir/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d8 "${testcvs} co -d dir 2d2mod" \
"${PROG} [a-z]*: Updating dir
U dir/file2"
	  dotest cvsadm-1d8b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-1d8d "cat dir/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  ##################################################
	  ## Los Combonaciones
	  ##################################################

	  ### 1mod

	  dotest cvsadm-1d9 "${testcvs} co -d dir 1mod 1mod-2" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1
${PROG} [a-z]*: Updating dir/1mod-2
U dir/1mod-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-1d9b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d9d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-1d9f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1mod copy
	  dotest cvsadm-1d9h "cat dir/1mod-2/CVS/Repository" \
"${AREP}mod1-2"
	  rm -rf CVS dir

	  # 1mod 2mod redmod bluemod
	  dotest cvsadm-1d10 "${testcvs} co -d dir 1mod 2mod" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1
${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2"
	  dotest cvsadm-1d10b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d10d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-1d10f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2dmod
	  dotest cvsadm-1d10h "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d11 "${testcvs} co -d dir 1mod 1d1mod" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1
${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1"
	  dotest cvsadm-1d11b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d11d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-1d11f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d1mod
	  dotest cvsadm-1d11h "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d12 "${testcvs} co -d dir 1mod 1d2mod" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1
${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-1d12b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d12d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-1d12f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-1d12h "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d13 "${testcvs} co -d dir 1mod 2d1mod" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1
${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d13b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d13d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-1d13f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-1d13h "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d13j "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d14 "${testcvs} co -d dir 1mod 2d2mod" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1
${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d14b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d14d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1mod
	  dotest cvsadm-1d14f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-1d14h "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d14j "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir


	  ### 2mod

	  dotest cvsadm-1d15 "${testcvs} co -d dir 2mod 2mod-2" \
"${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2
${PROG} [a-z]*: Updating dir/2mod-2
U dir/2mod-2/file2-2"
	  dotest cvsadm-1d15b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d15d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-1d15f "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2mod copy
	  dotest cvsadm-1d15h "cat dir/2mod-2/CVS/Repository" \
"${AREP}mod2-2/sub2-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d16 "${testcvs} co -d dir 2mod 1d1mod" \
"${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2
${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1"
	  dotest cvsadm-1d16b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d16d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-1d16f "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 1d1mod
	  dotest cvsadm-1d16h "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d17 "${testcvs} co -d dir 2mod 1d2mod" \
"${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2
${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-1d17b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d17d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-1d17f "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 1d2mod
	  dotest cvsadm-1d17h "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d18 "${testcvs} co -d dir 2mod 2d1mod" \
"${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2
${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d18b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d18d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-1d18f "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-1d18h "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d18j "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d19 "${testcvs} co -d dir 2mod 2d2mod" \
"${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2
${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d19b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d19d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2mod
	  dotest cvsadm-1d19f "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-1d19h "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d19j "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir


	  ### 1d1mod

	  dotest cvsadm-1d20 "${testcvs} co -d dir 1d1mod 1d1mod-2" \
"${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} [a-z]*: Updating dir/dir1d1-2
U dir/dir1d1-2/file1-2"
	  dotest cvsadm-1d20b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d20d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d20f "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d1mod copy
	  dotest cvsadm-1d20h "cat dir/dir1d1-2/CVS/Repository" \
"${AREP}mod1-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d21 "${testcvs} co -d dir 1d1mod 1d2mod" \
"${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-1d21b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d21d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d21f "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-1d21h "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d22 "${testcvs} co -d dir 1d1mod 2d1mod" \
"${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d22b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d22d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d22f "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-1d22h "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d22j "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d23 "${testcvs} co -d dir 1d1mod 2d2mod" \
"${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d23b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d23d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d23f "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-1d23h "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d23j "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir


	  ### 1d2mod

	  dotest cvsadm-1d24 "${testcvs} co -d dir 1d2mod 1d2mod-2" \
"${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2
${PROG} [a-z]*: Updating dir/dir1d2-2
U dir/dir1d2-2/file2-2"
	  dotest cvsadm-1d24b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d24d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d2mod
	  dotest cvsadm-1d24f "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 1d2mod copy
	  dotest cvsadm-1d24h "cat dir/dir1d2-2/CVS/Repository" \
"${AREP}mod2-2/sub2-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d25 "${testcvs} co -d dir 1d2mod 2d1mod" \
"${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2
${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d25b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d25d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d2mod
	  dotest cvsadm-1d25f "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-1d25h "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d25j "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d26 "${testcvs} co -d dir 1d2mod 2d2mod" \
"${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2
${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d26b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d26d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 1d2mod
	  dotest cvsadm-1d26f "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-1d26h "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d26j "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir


	  # 2d1mod

	  dotest cvsadm-1d27 "${testcvs} co -d dir 2d1mod 2d1mod-2" \
"${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1
${PROG} [a-z]*: Updating dir/dir2d1-2/sub2d1-2
U dir/dir2d1-2/sub2d1-2/file1-2"
	  dotest cvsadm-1d27b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d27d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2d1mod
	  dotest cvsadm-1d27f "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d27h "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-1d27j "cat dir/dir2d1-2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d27l "cat dir/dir2d1-2/sub2d1-2/CVS/Repository" \
"${AREP}mod1-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d28 "${testcvs} co -d dir 2d1mod 2d2mod" \
"${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1
${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d28b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d28d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2d1mod
	  dotest cvsadm-1d28f "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d28h "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-1d28j "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d28l "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  
	  # 2d2mod

	  dotest cvsadm-1d29 "${testcvs} co -d dir 2d2mod 2d2mod-2" \
"${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2
${PROG} [a-z]*: Updating dir/dir2d2-2/sub2d2-2
U dir/dir2d2-2/sub2d2-2/file2-2"
	  dotest cvsadm-1d29b "cat CVS/Repository" \
"${AREP}\."
	  # the usual for the dir level
	  dotest cvsadm-1d29d "cat dir/CVS/Repository" \
"${AREP}\."
	  # the usual for 2d2mod
	  dotest cvsadm-1d29f "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d29h "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-1d29j "cat dir/dir2d2-2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-1d29l "cat dir/dir2d2-2/sub2d2-2/CVS/Repository" \
"${AREP}mod2-2/sub2-2"
	  rm -rf CVS dir

	  ##################################################
	  ## And now, some of that again using the "-d" flag
	  ## on the command line, but use a longer path.
	  ##################################################

	  dotest cvsadm-2d3 "${testcvs} co -d dir/dir2 1mod" \
"${PROG} [a-z]*: Updating dir/dir2
U dir/dir2/file1"
	  dotest cvsadm-2d3b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-2d3d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-2d3f "cat dir/dir2/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-2d4 "${testcvs} co -d dir/dir2 2mod" \
"${PROG} [a-z]*: Updating dir/dir2
U dir/dir2/file2"
	  dotest cvsadm-2d4b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-2d4d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-2d4f "cat dir/dir2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-2d5 "${testcvs} co -d dir/dir2 1d1mod" \
"${PROG} [a-z]*: Updating dir/dir2
U dir/dir2/file1"
	  dotest cvsadm-2d5b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-2d5d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-2d5f "cat dir/dir2/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-2d6 "${testcvs} co -d dir/dir2 1d2mod" \
"${PROG} [a-z]*: Updating dir/dir2
U dir/dir2/file2"
	  dotest cvsadm-2d6b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-2d6d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-2d6f "cat dir/dir2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-2d7 "${testcvs} co -d dir/dir2 2d1mod" \
"${PROG} [a-z]*: Updating dir/dir2
U dir/dir2/file1"
	  dotest cvsadm-2d7b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-2d7d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-2d7f "cat dir/dir2/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-2d8 "${testcvs} co -d dir/dir2 2d2mod" \
"${PROG} [a-z]*: Updating dir/dir2
U dir/dir2/file2"
	  dotest cvsadm-2d8b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-2d8d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-2d8f "cat dir/dir2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  ##################################################
	  ## And now, a few of those tests revisited to
	  ## test the behavior of the -N flag.
	  ##################################################

	  dotest cvsadm-N3 "${testcvs} co -N 1mod" \
"${PROG} [a-z]*: Updating 1mod
U 1mod/file1"
	  dotest cvsadm-N3b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N3d "cat 1mod/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS 1mod

	  dotest cvsadm-N4 "${testcvs} co -N 2mod" \
"${PROG} [a-z]*: Updating 2mod
U 2mod/file2"
	  dotest cvsadm-N4b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N4d "cat 2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS 2mod

	  dotest cvsadm-N5 "${testcvs} co -N 1d1mod" \
"${PROG} [a-z]*: Updating dir1d1
U dir1d1/file1"
	  dotest cvsadm-N5b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N5d "cat dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir1d1

	  dotest cvsadm-N6 "${testcvs} co -N 1d2mod" \
"${PROG} [a-z]*: Updating dir1d2
U dir1d2/file2"
	  dotest cvsadm-N6b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N6d "cat dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir1d2

	  dotest cvsadm-N7 "${testcvs} co -N 2d1mod" \
"${PROG} [a-z]*: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  dotest cvsadm-N7b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N7d "cat dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N7f "cat dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir2d1

	  dotest cvsadm-N8 "${testcvs} co -N 2d2mod" \
"${PROG} [a-z]*: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  dotest cvsadm-N8b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N8d "cat dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N8f "cat dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir2d2

	  ## the ones in one-deep directories

	  dotest cvsadm-N1d3 "${testcvs} co -N -d dir 1mod" \
"${PROG} [a-z]*: Updating dir/1mod
U dir/1mod/file1"
	  dotest cvsadm-N1d3b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d3d "cat dir/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d3f "cat dir/1mod/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-N1d4 "${testcvs} co -N -d dir 2mod" \
"${PROG} [a-z]*: Updating dir/2mod
U dir/2mod/file2"
	  dotest cvsadm-N1d4b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d4d "cat dir/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d4f "cat dir/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-N1d5 "${testcvs} co -N -d dir 1d1mod" \
"${PROG} [a-z]*: Updating dir/dir1d1
U dir/dir1d1/file1"
	  dotest cvsadm-N1d5b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d5d "cat dir/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d5d "cat dir/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-N1d6 "${testcvs} co -N -d dir 1d2mod" \
"${PROG} [a-z]*: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-N1d6b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d6d "cat dir/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d6f "cat dir/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-N1d7 "${testcvs} co -N -d dir 2d1mod" \
"${PROG} [a-z]*: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-N1d7b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d7d "cat dir/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d7f "cat dir/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N1d7h "cat dir/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-N1d8 "${testcvs} co -N -d dir 2d2mod" \
"${PROG} [a-z]*: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-N1d8b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d8d "cat dir/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N1d8d "cat dir/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N1d8d "cat dir/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  ## the ones in two-deep directories

	  dotest cvsadm-N2d3 "${testcvs} co -N -d dir/dir2 1mod" \
"${PROG} [a-z]*: Updating dir/dir2/1mod
U dir/dir2/1mod/file1"
	  dotest cvsadm-N2d3b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d3d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d3f "cat dir/dir2/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d3h "cat dir/dir2/1mod/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-N2d4 "${testcvs} co -N -d dir/dir2 2mod" \
"${PROG} [a-z]*: Updating dir/dir2/2mod
U dir/dir2/2mod/file2"
	  dotest cvsadm-N2d4b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d4d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d4f "cat dir/dir2/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d4h "cat dir/dir2/2mod/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-N2d5 "${testcvs} co -N -d dir/dir2 1d1mod" \
"${PROG} [a-z]*: Updating dir/dir2/dir1d1
U dir/dir2/dir1d1/file1"
	  dotest cvsadm-N2d5b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d5d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d5f "cat dir/dir2/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d5h "cat dir/dir2/dir1d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-N2d6 "${testcvs} co -N -d dir/dir2 1d2mod" \
"${PROG} [a-z]*: Updating dir/dir2/dir1d2
U dir/dir2/dir1d2/file2"
	  dotest cvsadm-N2d6b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d6d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d6f "cat dir/dir2/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d6h "cat dir/dir2/dir1d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-N2d7 "${testcvs} co -N -d dir/dir2 2d1mod" \
"${PROG} [a-z]*: Updating dir/dir2/dir2d1/sub2d1
U dir/dir2/dir2d1/sub2d1/file1"
	  dotest cvsadm-N2d7b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d7d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d7f "cat dir/dir2/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d7f "cat dir/dir2/dir2d1/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d7h "cat dir/dir2/dir2d1/sub2d1/CVS/Repository" \
"${AREP}mod1"
	  rm -rf CVS dir

	  dotest cvsadm-N2d8 "${testcvs} co -N -d dir/dir2 2d2mod" \
"${PROG} [a-z]*: Updating dir/dir2/dir2d2/sub2d2
U dir/dir2/dir2d2/sub2d2/file2"
	  dotest cvsadm-N2d8b "cat CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d8d "cat dir/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d8f "cat dir/dir2/CVS/Repository" \
"${AREP}\."
	  dotest cvsadm-N2d8h "cat dir/dir2/dir2d2/CVS/Repository" \
"${AREP}CVSROOT/Emptydir"
	  dotest cvsadm-N2d8j "cat dir/dir2/dir2d2/sub2d2/CVS/Repository" \
"${AREP}mod2/sub2"
	  rm -rf CVS dir

	  ##################################################
	  ## That's enough of that, thank you very much.
	  ##################################################

	  # remove our junk
	  cd ..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/1mod
	  rm -rf ${CVSROOT_DIRNAME}/1mod-2
	  rm -rf ${CVSROOT_DIRNAME}/2mod
	  rm -rf ${CVSROOT_DIRNAME}/2mod-2
	  ;;

        diffmerge1)
	  # Make sure CVS can merge correctly in circumstances where it
	  # used to mess up (due to a bug which existed in diffutils 2.7
	  # and 2.6, but not 2.5, and which has been fixed in CVS's diff
	  # lib by Paul Eggert, bless his bitty heart).

	  # This first test involves two working copies, "mine" and
	  # "yours", checked out from the same repository at the same
	  # time.  In yours, you remove some text from the end of the
	  # file and check it in; meanwhile, "me" has commented out some
	  # lines earlier in the file, and I go to check it in right
	  # after you checked yours in.  CVS naturally tells me the file
	  # is not up-to-date, so I run cvs update, but it updates
	  # incorrectly, leaving in the lines of text you just deleted.
	  # Bad!  I'm in too much of a hurry to actually look at the
	  # file, so I check it in and go home, and so your changes have
	  # been lost.  Later you discover this, and you suspect me of
	  # deliberately sabotaging your work, so you let all the air
	  # out of my tires.  Only after a series of expensive lawsuits
	  # and countersuits do we discover it this was all CVS's
	  # fault.
	  #
	  # Luckily, this problem has been fixed now, as our test will
	  # handily confirm, no doubt:

	  # First make a repository containing the original text:

	  # We should be here anyway, but cd to it just in case:
	  cd ${TESTDIR}

	  mkdir diffmerge1
	  cd diffmerge1

	  # The text of the file is inlined here because `echo' loses
	  # newlines, and I don't know how portable the -e flag is.
	  # 
	  # This is the file we both start out with:
	  echo "// Button.java

package random.application;

import random.util.star;

public class Button
{
  /star Instantiates a Button with origin (0, 0) and zero width and height.
   star You must call an initializer method to properly initialize the Button.
   star/
  public Button ()
  {
    super ();

    _titleColor = Color.black;
    _disabledTitleColor = Color.gray;
    _titleFont = Font.defaultFont ();
  }

  /star Convenience constructor for instantiating a Button with
   star bounds x, y, width, and height.  Equivalent to
   star     foo = new Button ();
   star     foo.init (x, y, width, height);
   star/
  public Button (int x, int y, int width, int height)
  {
    this ();
    init (x, y, width, height);
  }
}" > the_file

	  dotest diffmerge1_import \
	    "${testcvs} import -m import diffmerge1 tag1 tag2" \
	    "${DOTSTAR}No conflicts created by this import"
	  cd ..
	  rm -rf diffmerge1

	  # Check out two working copies, one for "you" and one for "me"
	  ${testcvs} checkout diffmerge1 >/dev/null 2>&1
	  mv diffmerge1 diffmerge1_yours
	  ${testcvs} checkout diffmerge1 >/dev/null 2>&1
	  mv diffmerge1 diffmerge1_mine

	  # In your working copy, you'll remove the Button() method, and
	  # then check in your change before I check in mine:
	  cd diffmerge1_yours
	  echo "// Button.java

package random.application;

import random.util.star;

public class Button
{
  /star Instantiates a Button with origin (0, 0) and zero width and height.
   star You must call an initializer method to properly initialize the Button.
   star/
  public Button ()
  {
    super ();

    _titleColor = Color.black;
    _disabledTitleColor = Color.gray;
    _titleFont = Font.defaultFont ();
  }
}" > the_file
	  dotest diffmerge1_yours \
	    "${testcvs} ci -m yours" \
	    "${DOTSTAR}hecking in ${DOTSTAR}"

	  # My working copy still has the Button() method, but I
	  # comment out some code at the top of the class.  Then I
	  # update, after both my modifications and your checkin:
	  cd ../diffmerge1_mine
	  echo "// Button.java

package random.application;

import random.util.star;

public class Button
{
  /star Instantiates a Button with origin (0, 0) and zero width and height.
   star You must call an initializer method to properly initialize the Button.
   star/
  public Button ()
  {
    super ();

    // _titleColor = Color.black;
    // _disabledTitleColor = Color.gray;
    // _titleFont = Font.defaultFont ();
  }

  /star Convenience constructor for instantiating a Button with
   star bounds x, y, width, and height.  Equivalent to
   star     foo = new Button ();
   star     foo.init (x, y, width, height);
   star/
  public Button (int x, int y, int width, int height)
  {
    this ();
    init (x, y, width, height);
  }
}" > the_file
	  dotest diffmerge1_mine \
	    "${testcvs} update" \
	    "${DOTSTAR}erging${DOTSTAR}"

	  # So if your changes didn't make it into my working copy, or
	  # in any case if the file does not look like the final text as
	  # quoted below, then the test flunks:
	  echo "// Button.java

package random.application;

import random.util.star;

public class Button
{
  /star Instantiates a Button with origin (0, 0) and zero width and height.
   star You must call an initializer method to properly initialize the Button.
   star/
  public Button ()
  {
    super ();

    // _titleColor = Color.black;
    // _disabledTitleColor = Color.gray;
    // _titleFont = Font.defaultFont ();
  }
}" > comp_me
	  dotest diffmerge1_cmp "cmp the_file comp_me" ''

	  # Clean up after ourselves:
	  cd ..
	  rm -rf diffmerge1_yours diffmerge1_mine ${CVSROOT_DIRNAME}/diffmerge1

	  ;;

        diffmerge2)

	  # FIXME: This test should be rewritten to be much more concise.
	  # It currently weighs in at something like 600 lines, but the
	  # same thing could probably be tested in more like 50-100 lines.
	  mkdir diffmerge2

	  # This tests for another diffmerge bug reported by Martin
	  # Tomes; actually, his bug was probably caused by an initial
	  # fix for the bug in test diffmerge1, and likely wasn't ever
	  # a problem in CVS as long as one was using a normal
	  # distribution of diff or a version of CVS that has the diff
	  # lib in it. 
	  #
	  # Nevertheless, once burned twice cautious, so we test for his
	  # bug here.
	  #
	  # Here is his report, more or less verbatim:
	  # ------------------------------------------
	  #
	  # Put the attached file (sgrid.h,v) into your repository
	  # somewhere, check out the module and do this:
	  #
	  # cvs update -j Review_Phase_2_Enhancements sgrid.h
	  # cvs diff -r Review_V1p3 sgrid.h
	  #
	  # As there have been no changes made on the trunk there
	  # should be no differences, however this is output:
	  #
	  # % cvs diff -r Review_V1p3 sgrid.h
	  # Index: sgrid.h
	  # ===================================================================
	  # RCS file: /usr/local/repository/play/fred/sgrid.h,v
	  # retrieving revision 1.1.2.1
	  # diff -r1.1.2.1 sgrid.h
	  # 178a179,184
	  # > /*--------------------------------------------------------------
	  # > INLINE FUNCTION    :    HORIZONTALLINES
	  # > NOTES              :    Description at the end of the file
	  # > ----------------------------------------------------------------*/
	  # >         uint16 horizontalLines( void );
	  # >
	  #
	  # I did a cvs diff -c -r 1.1 -r 1.1.2.1 sgrid.h and patched those
	  # differences to sgrid.h version 1.1 and got the correct result
	  # so it looks like the built in patch is faulty.
	  # -------------------------------------------------------------------
	  #
	  # This is the RCS file, sgrid.h,v, that he sent:

	  echo "head	1.1;
access;
symbols
	Review_V1p3:1.1.2.1
	Review_V1p3C:1.1.2.1
	Review_1p3A:1.1.2.1
	Review_V1p3A:1.1.2.1
	Review_Phase_2_Enhancements:1.1.0.2
	Review_V1p2:1.1
	Review_V1p2B:1.1
	Review_V1p2A:1.1
	Review_V1p1:1.1
	Review_1p1:1.1;
locks; strict;
comment	@ * @;


1.1
date	97.04.02.11.20.05;	author colinl;	state Exp;
branches
	1.1.2.1;
next	;

1.1.2.1
date	97.06.09.10.00.07;	author colinl;	state Exp;
branches;
next	;


desc
@@


1.1
log
@Project:     DEV1175
DCN:
Tested By:   Colin Law
Reviewed By:
Reason for Change: Initial Revision of all files

Design Change Details:

Implications:
@
text
@/* \$""Header:   L:/gpanels/dis/sgrid.h_v   1.1.1.0   24 Jan 1996 14:59:20   PAULT  \$ */
/*
 * \$""Log:   L:/gpanels/dis/sgrid.h_v  \$
 * 
 *    Rev 1.1.1.0   24 Jan 1996 14:59:20   PAULT
 * Branched
 * 
 *    Rev 1.1   24 Jan 1996 12:09:52   PAULT
 * Consolidated 4100 code merged to trunk
 * 
 *    Rev 1.0.2.0   01 Jun 1995 14:18:58   DAVEH
 * Branched
 * 
 *    Rev 1.0   19 Apr 1995 16:32:48   COLINL
 * Initial revision.
*/
/*****************************************************************************
FILE        :   SGRID.H
VERSION     :   2.1
AUTHOR      :   Dave Hartley
SYSTEM      :   Borland C++
DESCRIPTION :   The declaration of the scrolling grid class
                  
*****************************************************************************/
#if !defined(__SGRID_H)
#define __SGRID_H

#if !defined(__SCROLL_H)
#include <scroll.h>
#endif

#if !defined(__GKI_H)
#include \"gki.h\"
#endif

#if defined PRINTING_SUPPORT
class Printer;
#endif

/*****************************************************************************
CLASS      :    ScrollingGrid   
DESCRIPTION:    This class inherits from a grid and a scrollable, and
                can therefore use all the PUBLIC services provided by these
                classes. A description of these can be found in
                GRID.H and SCROLL.H.
                A scrolling grid is a set of horizontal and vertical lines
                that scroll and continually update to provide a complete grid

*****************************************************************************/

class ScrollingGrid : public Scrollable
{
    public:
#if defined _WINDOWS
/*---------------------------------------------------------------------------
FUNCTION    :   CONSTRUCTOR
DESCRIPTION :   sets up the details of the grid, ready for painting
ARGUMENTS   :   name  : sgColour
                        - the colour of the grid
                        sgLineType
                        - the syle of line
                        sgHorizontalTotal
                        - the total number of horizontal grid lines
                        verticalSpacingMin
                        - the min distance between the vertical grid lines
                          on the scrolling axis
                        currentTimestamp
                        - timestamp value now
                        ticksPerSecond
                        - number of timestamp ticks per second
                        ticksPerPixel
                        - number of timestamp ticks per pixel required
                      
RETURN      :   None
NOTES       :   
---------------------------------------------------------------------------*/
        ScrollingGrid( GkiColour sgColour, GkiLineType sgLineType, 
            uint16 sgHorizontalTotal, 
            uint16 verticalSpacingMin, uint32 currentTimestamp, 
            uint16 ticksPerSecond, uint32 ticksPerPixel );
#else
/*---------------------------------------------------------------------------
FUNCTION    :   CONSTRUCTOR
DESCRIPTION :   sets up the details of the grid, ready for painting
ARGUMENTS   :   name  : sgColour
                        - the colour of the grid
                        sgLineType
                        - the syle of line
                        sgHorizontalTotal ( THE MAX NUMBER OF LINES IS 100 )
                        - the total number of horizontal grid lines
                        sgVerticalSpacing
                        - the distance between the vertical grid lines
                        on the scrolling axis
                      
RETURN      :   None
NOTES       :   If the caller does not get the total grid lines value, synced
                with the overall size of the viewport, the spacing between
                grid lines will not be consistent.

---------------------------------------------------------------------------*/
        ScrollingGrid( GkiColour sgColour, GkiLineType sgLineType
                     , uint16 sgHorizontalTotal, uint16 sgVerticalSpacing );
#endif
/*---------------------------------------------------------------------------
FUNCTION    :   DESTRUCTOR
DESCRIPTION :   tidies it all up
ARGUMENTS   :   name  :      
                      
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        ~ScrollingGrid( void );

/*---------------------------------------------------------------------------
FUNCTION    :   ATTACH
DESCRIPTION :   This service overloads the base class service, as it does
                additional work at the time of attachment.

ARGUMENTS   :   name  : tDrawingArea
                        - the scrolled viewport to attach this trend to
                      
RETURN      :   None
NOTES       :
---------------------------------------------------------------------------*/
        void attach( SViewport *tDrawingArea );

#if defined _WINDOWS
/*---------------------------------------------------------------------------
FUNCTION    :   calculateVerticalSpacing
DESCRIPTION :   determines optimum spacing along time axis
ARGUMENTS   :   
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void calculateVerticalSpacing();

/*---------------------------------------------------------------------------
FUNCTION    :   gridSpacingTicks
DESCRIPTION :   Provides the grid spacing in the time axis in ticks
ARGUMENTS   :   
RETURN      :   Number of ticks
NOTES       : 
---------------------------------------------------------------------------*/
        uint32 gridSpacingTicks();

#endif

/*---------------------------------------------------------------------------
INLINE FUNCTION    :    HORIZONTALLINES
NOTES              :    Description at the end of the file
---------------------------------------------------------------------------*/
        uint16 horizontalLines( void );

#if defined _WINDOWS
// In Windows the OnDraw() function replaces paint()
/*---------------------------------------------------------------------------
FUNCTION    :   ScrollingGrid OnDraw   
DESCRIPTION :   Paints the given area of the grid.
                Pure virtual
ARGUMENTS   :   pDC     pointer to the device context to use for display
                        Note that the device context operates in the coords
                        of the window owning the viewport
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        virtual void OnDraw( CDC *pDC );

#else   // not Windows            

/*---------------------------------------------------------------------------
FUNCTION    :   PAINT
DESCRIPTION :   This extends the standard grid paint method to paint the
                viewport relative to its current position. 
                
ARGUMENTS   :   name  :      
                      
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void paint( void );
#endif

/*---------------------------------------------------------------------------
FUNCTION    :   P A I N T   T E X T   M A R K E R S 
DESCRIPTION :   this service allow the text markers to be painted seperatley
                from the grid lines

ARGUMENTS   :   name : 
                                                                          
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void paintTextMarkers();

#if defined PRINTING_SUPPORT
/*---------------------------------------------------------------------------
FUNCTION    :   P R I N T 
DESCRIPTION :   This print service prints a grid marker ( being either a
                timestamp or a date, IF there is one at the plot position
                given

ARGUMENTS   :   name :
                        displayPosition
                        - Where in the log to look to see if there is an
                          entry to print

                        - printerPtr
                          the printer to print to
                                                                          
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void print( uint16 currentPrintPos, Printer *printerPtr );
#endif

/*---------------------------------------------------------------------------
FUNCTION    :   S E T  D R I V E  D I R E C T I O N
DESCRIPTION :   Sets direction for update and scrolling forwards or backwards
ARGUMENTS   :   direction  - required direction
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void setDriveDirection( ScrollDirection direction );

/*---------------------------------------------------------------------------
FUNCTION    :   S E T U P 
DESCRIPTION :   service that will setup the grid prior to a paint

ARGUMENTS   :   name :
                        - newTimestamp
                            

                        - newTimeBase
                        the number of ticks that represent a plot point on
                        the trendgraph. 
                                                                          
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void setup( uint32 newTimestamp, uint32 newTimeBase );

#if defined PRINTING_SUPPORT
/*---------------------------------------------------------------------------
FUNCTION    :   S E T U P   F O R   P R I N T   
DESCRIPTION :   This service iis to be called prior to printing. It allows
                the grid to prepare its markers ready for the print
                commands

ARGUMENTS   :   name : 
                                                                          
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void setupForPrint();
#endif

/*---------------------------------------------------------------------------
FUNCTION    :   UPDATE
DESCRIPTION :   When this service is called it will calculate what needs to
                be painted and fill in the display again.

ARGUMENTS   :   name  :     timeStamp
                            - the reference time of this update.
                      
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void update( uint32 timeStamp );

/*---------------------------------------------------------------------------
FUNCTION    :   U P D A T E   B U F F E R
DESCRIPTION :   When a display update is not required, use this method. It
                updates the internal data ready for a call to paint that
                will then show the grid in the right position

ARGUMENTS   :   name  :      
                      
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void updateBuffer( void );

    private:

/*---------------------------------------------------------------------------
FUNCTION    :   M A K E   G R I D   M A R K E R 
DESCRIPTION :   service that perpares a string for display. The string will
                either be a short date, or short time. this is determined
                by the current setting of the dateMarker flag

ARGUMENTS   :   name :  timestampVal
                        - the value to convert
                        
                        storePtr
                        - the place to put the string

RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void makeGridMarker( uint32 timestampVal, char *storePtr );
            
/*---------------------------------------------------------------------------
FUNCTION    :   P A I N T   G R I D   M A R K E R 
DESCRIPTION :   given a position will put the string on the display

ARGUMENTS   :   name :
                        yPos
                        - were it goes on the Y-axis

                        gridMarkerPtr
                        - what it is
                                                                          
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
        void paintGridMarker( uint16 yPos, char *gridMarkerPtr );

#if defined _WINDOWS
/*---------------------------------------------------------------------------
FUNCTION    :   PAINTHORIZONTALLINES
DESCRIPTION :   responsible for painting the grids horizontal lines 
ARGUMENTS   :   pRectToDraw     pointer to rectangle that needs refreshing.
                                in viewport coords
                pDC             pointer to device context to use
                      
RETURN      : None
NOTES       :
---------------------------------------------------------------------------*/
        void paintHorizontalLines(RectCoords* pRectToDraw, CDC* pDC );
#else
/*---------------------------------------------------------------------------
FUNCTION    :   PAINTHORIZONTALLINES
DESCRIPTION :   responsible for painting the grids horizontal lines 
ARGUMENTS   : name: xStart
                    - the starting X co-ordinate for the horizontal line
                    xEnd
                    - the ending X co-ordinate for the horizontal line
                      
RETURN      : None
NOTES       : Remember lines are drawn from origin. The origin in a
              horizontal viewport will be the top.    
---------------------------------------------------------------------------*/
        void paintHorizontalLines( uint16 xStart, uint16 xEnd );
#endif

#if defined _WINDOWS
/*---------------------------------------------------------------------------
FUNCTION    :   PAINTVERTICALLINES
DESCRIPTION :   responsible for painting the grids vertical lines 
ARGUMENTS   :   pRectToDraw     pointer to rectangle that needs refreshing.
                                in viewport coords
                offset          offset from rhs that rightmost line would be 
                                drawn if rectangle included whole viewport
                pDC             pointer to device context to use
RETURN      : None
NOTES       : 
---------------------------------------------------------------------------*/
        void paintVerticalLines( RectCoords* pRectToDraw, uint16 offset,
            CDC* pDC );
#else
/*---------------------------------------------------------------------------
FUNCTION    :   PAINTVERTICALLINES
DESCRIPTION :   responsible for painting the grids vertical lines 
ARGUMENTS   : name  :   yStart
                        - the starting Y co-ordinate for the vertical line
                        yEnd
                        - the ending Y co-ordinate for the vertical line
                        offset
                        - a starting point offset that determines at what X
                        position the first line will be drawn

                      
RETURN      : None
NOTES       : 
---------------------------------------------------------------------------*/
        void paintVerticalLines( uint16 yStart, uint16 yEnd, uint16 offset );
#endif

#if defined _WINDOWS
/*---------------------------------------------------------------------------
FUNCTION    :   PAINTVERTICALLINE
DESCRIPTION :   paints one line at the position specified, and length
ARGUMENTS   :   name  : yStart
                        - the starting point on the y axis for the line
                        yEnd
                        - the end point on the y axis for the line
                        xPosition
                        - The horizontal offset from the start of the viewport
                pDC             pointer to device context to use
                      
RETURN      :   None
NOTES       :   There is not an equivalent horizontal method as yet. This
                is a seperate method because the service is useful to a
                derivation of this class
---------------------------------------------------------------------------*/
        void paintVerticalLine( uint16 yStart, uint16 yEnd
                              , uint16 xPosition, CDC *pDC );
#else
/*---------------------------------------------------------------------------
FUNCTION    :   PAINTVERTICALLINE
DESCRIPTION :   paints one line at the position specified, and length
ARGUMENTS   :   name  : yStart
                        - the starting point on the y axis for the line
                        yEnd
                        - the end point on the y axis for the line
                        xPosition
                        - The horizontal offset from the start of the viewport
                      
RETURN      :   None
NOTES       :   There is not an equivalent horizontal method as yet. This
                is a seperate method because the service is useful to a
                derivation of this class
---------------------------------------------------------------------------*/
        void paintVerticalLine( uint16 yStart, uint16 yEnd
                              , uint16 xPosition );
#endif

/*---------------------------------------------------------------------------
INLINE FUNCTION    :    VERTICALSPACING
NOTES              :    Description at the end of the file
---------------------------------------------------------------------------*/
        uint16 verticalSpacing( void );


        // Position in viewport that we are now writing to if going forwards
        // Note that if this is greater than viewport length then we have
        // just scrolled and value must be adjusted before use.
        sint16 forwardsOutputPosition;
        
        // Position in viewport that we are now writing to if going backwards
        // Note that if this is less than zero then we have
        // just scrolled and value must be adjusted before use.
        sint16 backwardsOutputPosition;

        // position in grid cycle of forwards output position.
        // if zero then it is time to output a grid line
        sint16 forwardsIntervalCount;

        // position in grid cycle of forwards output position.
        // if zero then it is time to output a grid line
        sint16 backwardsIntervalCount;
        
        uint32  lastUpdateTimestamp;
        uint32  timeBase;       // ticks per pixel
        uint16  currentOutputPosition;
        uint16  gridTimestampSpacing;
        uint16  intervalCount;
        uint16  horizontalTotal;
        uint16  vSpacing;
#if defined PRINTING_SUPPORT
        uint16  numberOfGridMarkersPrinted;
#endif
        bool    firstTime;       // indicates first time through
        bool    dateMarker;

        GkiLineType lineType;
        GkiColour   gridColour;

    #if defined _WINDOWS
        uint16 ticksPerSec;     // number of time ticks per second
        uint16 vSpacingMin;     // minimum pixels per division along time axis 
        CPen *pPen;             // the pen to use for drawing in windows
    #endif

};


/*****************************************************************************
                        I N L I N E   F U N C T I O N S   
*****************************************************************************/

/*---------------------------------------------------------------------------
FUNCTION    :   HORIZONTALLINES
DESCRIPTION :   supplies the number of horizontal lines in the grid
ARGUMENTS   :   name  :      
                      
RETURN      :   
NOTES       : 
---------------------------------------------------------------------------*/
inline uint16 ScrollingGrid::horizontalLines( void )
{
    return( horizontalTotal );
}
/*---------------------------------------------------------------------------
FUNCTION    :   VERTICALSPACING
DESCRIPTION :   returns the distance between adjacent vertical lines
ARGUMENTS   :   name  :      
                      
RETURN      :   None
NOTES       : 
---------------------------------------------------------------------------*/
inline uint16 ScrollingGrid::verticalSpacing( void )
{
    return( vSpacing );
}

#endif
@


1.1.2.1
log
@DEV1194:DS4    Provision of major and minor grid lines
@
text
@d1 1
a1 1
/* \$""Header: /usr/local/repository/cmnsrc/review/src/sgrid.h,v 1.1 1997/04/02 11:20:05 colinl Exp \$ */
d3 1
a3 12
 * \$""Log: sgrid.h,v \$
 * Revision 1.1  1997/04/02 11:20:05  colinl
 * Project:     DEV1175
 * DCN:
 * Tested By:   Colin Law
 * Reviewed By:
 * Reason for Change: Initial Revision of all files
 *
 * Design Change Details:
 *
 * Implications:
 *
d58 6
a63 5
ARGUMENTS   :   name  : majorColour         colour for major grid lines
                        minorColour         colour for minor grid lines
                        sgLineType          line type for minor grid lines
                        yMajorGridLines     number of major y lines on grid
                        yMinorGridLines     number of major y lines on grid
d77 2
a78 3
        ScrollingGrid( GkiColour majorColour, GkiColour minorColour, 
            GkiLineType sgLineType, 
            uint16 yMajorGridLines, uint16 yMinorGridLines,
a137 17
FUNCTION    :   DrawHorizontalGridLines

DESCRIPTION :   Draws major or minor grid lines
ARGUMENTS   :   pDC         device context
                pPen        pen to use
                numLines    total lines required
                yLow, yHigh, xLow, xHigh   rectangle to draw in
                yMax        max y value
RETURN      :   None
NOTES       :   
---------------------------------------------------------------------------*/
        void DrawHorizontalGridLines( CDC* pDC, CPen* pPen, 
            uint16 numLines,
            uint16 yLow, uint16 yHigh, uint16 xLow, uint16 xHigh, 
            uint16 yMax );

/*---------------------------------------------------------------------------
d148 6
d448 1
a448 2
        uint16  m_yMajorGridLines;
        uint16  m_yMinorGridLines;
d456 2
a457 3
        GkiLineType lineType;    // line type for minor grid lines
        GkiColour   m_majorColour;
        GkiColour   m_minorColour;
d462 1
a462 2
        CPen *pMajorPen;        // pen to use for drawing major grid lines
        CPen *pMinorPen;        // pen to use for drawing minor grid lines
d472 12
@" > diffmerge2/sgrid.h,v

	  # We have to put the RCS file in the repository by hand for
	  # this test:
	  mkdir ${CVSROOT_DIRNAME}/diffmerge2
	  cp diffmerge2/sgrid.h,v ${CVSROOT_DIRNAME}/diffmerge2/sgrid.h,v
	  rm -rf diffmerge2
	  dotest diffmerge2_co \
	    "${testcvs} co diffmerge2" "${DOTSTAR}U ${DOTSTAR}"
	  cd diffmerge2
	  dotest diffmerge2_update \
	    "${testcvs} update -j Review_Phase_2_Enhancements sgrid.h" \
	    "${DOTSTAR}erging ${DOTSTAR}"
	  # This is the one that counts -- there should be no output:
	  dotest diffmerge2_diff \
	    "${testcvs} diff -r Review_V1p3 sgrid.h" ''

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
# * Test the contents of adm files other than Root and Repository.
#   Entries seems the next most important thing.
# End of TODO list.

# Remove the test directory, but first change out of it.
cd /tmp
rm -rf ${TESTDIR}

# end of sanity.sh
