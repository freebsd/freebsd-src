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

# usage:
usage ()
{
    echo "Usage: `basename $0` --help"
    echo "Usage: `basename $0` [-eklrv] [-f FROM-TEST] [-h HOSTNAME] CVS-TO-TEST [TESTS-TO-RUN...]"
}

exit_usage ()
{
    usage 1>&2
    exit 2
}

exit_help ()
{
    usage
    echo
    echo "-H|--help     Display this text."
    echo "-e|--skipfail Treat tests that would otherwise be nonfatally skipped"
    echo "              for reasons like missing tools as failures, exiting"
    echo "              with an error message.  Also treat warnings as"
    echo "              failures."
    echo "-f FROM-TEST  Run TESTS-TO-RUN, skipping all tests in the list before"
    echo "              FROM-TEST."
    echo "-h HOSTNAME   Use :ext:HOSTNAME to run remote tests rather than"
    echo "              :fork:.  Implies --remote and assumes that \$TESTDIR"
    echo "              resolves to the same directory on both the client and"
    echo "              the server."
    echo "-k|--keep     Try to keep directories created by individual tests"
    echo "              around, exiting after the first test which supports"
    echo "              --keep."
    echo "-l|--link-root"
    echo "              Test CVS using a symlink to a real CVSROOT."
    echo "-r|--remote   Test remote instead of local cvs."
    echo "-v|--verbose  List test names as they are executed."
    echo
    echo "CVS-TO-TEST   The path to the CVS executable to be tested."
    echo "TESTS-TO-RUN  The names of the tests to run (defaults to all tests)."
    exit 2
}

# See TODO list at end of file.

# required to make this script work properly.
unset CVSREAD

# This will cause malloc to run slower but should also catch some common errors
# when CVS is linked with glibc 2.x.
MALLOC_CHECK_=2; export MALLOC_CHECK_

# We want to invoke a predictable set of i18n behaviors, not whatever
# the user running this script might have set.
# In particular:
#   'sort' and tabs and spaces (LC_COLLATE).
#   Messages from getopt (LC_MESSAGES) (in the future, CVS itself might 
#     also alter its messages based on LC_MESSAGES).
LANG=C
export LANG
LC_ALL=C
export LC_ALL


#
# Initialize the test counts.
#
passed=0
skipped=0
warnings=0



#
# read our options
#
unset fromtest
unset remotehost
keep=false
linkroot=false
remote=false
skipfail=false
verbose=false
while getopts ef:h:Hklrv-: option ; do
    # convert the long opts to short opts
    if test x$option = x-;  then
	case "$OPTARG" in
	    [hH]|[hH][eE]|[hH][eE][lL]|[hH][eE][lL][pP])
		option=H;
		OPTARG=
		;;
	    [kK]|[kK][eE]|[kK][eE][eE]|[kK][eE][eE][pP])
		option=k;
		OPTARG=
		;;
	    l|li|lin|link|link-|link-r]|link-ro|link-roo|link-root)
		option=l;
		OPTARG=
		;;
	    [rR]|[rR][eE]|[rR][eE][mM]|[rR][eE][mM][oO]|[rR][eE][mM][oO][tT]|[rR][eE][mM][oO][tT][eE])
		option=k;
		OPTARG=
		;;
	    s|sk|ski|skip|skipf|skipfa|skipfai|skipfail)
		option=e
		OPTARG=
		;;
	    v|ve|ver|verb|verbo|verbos|verbose)
		option=v
		OPTARG=
		;;
	    *)
		option=\?
		OPTARG=
	esac
    fi
    case "$option" in
	e)
	    skipfail=:
	    ;;
	f)
	    fromtest="$OPTARG"
	    ;;
	h)
	    # Set a remotehost to run the remote tests on via :ext:
	    # Implies `-r' and assumes that $TESTDIR resolves to the same
	    # directory on the client and the server.
	    remotehost="$OPTARG"
	    remote=:
	    ;;
	H)
	    exit_help
	    ;;
	k)
	    # The -k (keep) option will eventually cause all the tests to
	    # leave around the contents of the /tmp directory; right now only
	    # some implement it.  Not originally intended to be useful with
	    # more than one test, but this should work if each test uses a
	    # uniquely named dir (use the name of the test).
	    keep=:
	    ;;
	l)
	    linkroot=:
	    ;;
	r)
	    remote=:
	    ;;
	v)
	    verbose=:
	    ;;
	\?)
	    exit_usage
	    ;;
    esac
done

# boot the arguments we used above
while test $OPTIND -gt 1 ; do
    shift
    OPTIND=`expr $OPTIND - 1`
done

# Use full path for CVS executable, so that CVS_SERVER gets set properly
# for remote.
case $1 in
"")
  exit_usage
  ;;
/*)
  testcvs=$1
  ;;
*)
  testcvs=`pwd`/$1
  ;;
esac
shift

# If $remotehost is set, warn if $TESTDIR isn't since we are pretty sure
# that its default value of `/tmp/cvs-sanity' will not resolve to the same
# directory on two different machines.
if test -n "$remotehost" && test -z "$TESTDIR"; then
    echo "WARNING: CVS server hostname is set and \$TESTDIR is not.  If" >&2
    echo "$remotehost is not the local machine, then it is unlikely that" >&2
    echo "the default value assigned to \$TESTDIR will resolve to the same" >&2
    echo "directory on both this client and the CVS server." >&2
fi



###
### GUTS
###

# "debugger"
#set -x

echo 'This test should produce no other output than this message, and a final "OK".'
echo '(Note that the test can take an hour or more to run and periodically stops'
echo 'for as long as one minute.  Do not assume there is a problem just because'
echo 'nothing seems to happen for a long time.  If you cannot live without'
echo 'running status, use the -v option or try the command:'
echo "\`tail -f check.log' from another window.)"

# Regexp to match what CVS will call itself in output that it prints.
# FIXME: we don't properly quote this--if the name contains . we'll
# just spuriously match a few things; if the name contains other regexp
# special characters we are probably in big trouble.
PROG=`basename ${testcvs}`

# Match the hostname
hostname="[-_.a-zA-Z0-9]*"

# Regexp to match the name of a temporary file (from cvs_temp_name).
# This appears in certain diff output.
tempname="[-a-zA-Z0-9/.%_]*"

# Regexp to match a date in RFC822 format (as amended by RFC1123).
RFCDATE="[a-zA-Z0-9 ][a-zA-Z0-9 ]* [0-9:][0-9:]* -0000"
RFCDATE_EPOCH="1 Jan 1970 00:00:00 -0000"

# Regexp to match a date in standard Unix format as used by rdiff
# FIXCVS: There's no reason for rdiff to use a different date format
# than diff does
DATE="[a-zA-Z]* [a-zA-Z]* [ 1-3][0-9] [0-9:]* [0-9]*"

# Which directories should Which and find_tool search for executables?
SEARCHPATH=$PATH:/usr/local/bin:/usr/contrib/bin:/usr/contrib:/usr/gnu/bin:/local/bin:/local/gnu/bin:/gnu/bin:/sw/bin:/usr/pkg/bin

# Do not assume that `type -p cmd` is portable
# Usage: Which [-a] [-x|-f|-r] prog [$SEARCHPATH:/with/directories:/to/search]
Which() {
  # Optional first argument for file type, defaults to -x.
  # Second argument is the file or directory to be found.
  # Third argument is the PATH to search.
  # By default, print only the first file that matches,
  # -a will cause all matches to be printed.
  notevery=:
  if [ "x$1" = "x-a" ]; then notevery=false; shift; fi
  case "$1" in
    -*) t=$1; shift ;;
    *) t=-x ;;
  esac
  case "$1" in
    # FIXME: Someday this may need to be fixed
    # to deal better with C:\some\path\to\ssh values...
    /*) test $t $1 && echo $1 ;;
    *) for d in `IFS=:; echo ${2-$SEARCHPATH}`
       do
         test $t $d/$1 && { echo $d/$1; if $notevery; then break; fi; }
       done
       ;;
  esac
}


# On cygwin32, we may not have /bin/sh.
if test -r /bin/sh; then
  TESTSHELL="/bin/sh"
else
  TESTSHELL=`Which -f sh`
  if test ! -r "$TESTSHELL"; then
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

# Create the log file so check.log can be tailed almost immediately after
# this script is started.  Otherwise it can take up to a minute or two before
# the log file gets created when $remotehost is specified on some systems,
# which makes for a lot of failed `tail -f' attempts.
touch check.log

# Workaround any X11Forwarding by ssh. Otherwise this text:
#   Warning: No xauth data; using fake authentication data for X11 forwarding.
# has been known to end up in the test results below
# causing the test to fail.
[ -n "$DISPLAY" ] && unset DISPLAY
  
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
: ${CVS_RSH=rsh}; export CVS_RSH
if test -n "$remotehost"; then
        # We need to set $tmp on the server since $TMPDIR is compared against
	# messages generated by the server.
	tmp=`$CVS_RSH $remotehost 'cd /tmp; /bin/pwd || pwd' 2>/dev/null`
	if test $? != 0; then
	    echo "$CVS_RSH $remotehost failed." >&2
	    exit 1
	fi
else
	tmp=`(cd /tmp; /bin/pwd || pwd) 2>/dev/null`
fi

# Now:
#	1) Set TESTDIR if it's not set already
#	2) Remove any old test remnants
#	3) Create $TESTDIR
#	4) Normalize TESTDIR with `cd && (/bin/pwd || pwd)`
#	   (This will match CVS output later)
: ${TESTDIR=$tmp/cvs-sanity}
# clean any old remnants (we need the chmod because some tests make
# directories read-only)
if test -d ${TESTDIR}; then
    chmod -R a+wx ${TESTDIR}
    rm -rf ${TESTDIR}
fi
# These exits are important.  The first time I tried this, if the `mkdir && cd`
# failed then the build directory would get blown away.  Some people probably
# wouldn't appreciate that.
mkdir ${TESTDIR} || exit 1
cd ${TESTDIR} || exit 1
# Ensure $TESTDIR is absolute
if echo "${TESTDIR}" |grep '^[^/]'; then
    # Don't resolve this unless we have to.  This keeps symlinks intact.  This
    # is important at least when testing using -h $remotehost, because the same
    # value for $TESTDIR must resolve to the same directory on the client and
    # the server and we likely used Samba, and possibly symlinks, to do this.
    TESTDIR=`(/bin/pwd || pwd) 2>/dev/null`
fi

if test -z "${TESTDIR}" || echo "${TESTDIR}" |grep '^[^/]'; then
    echo "Unable to resolve TESTDIR to an absolute directory." >&2
    exit 1
fi
cd ${TESTDIR}

# Now set $TMPDIR if the user hasn't overridden it.
#
# We use a $TMPDIR under $TESTDIR by default so that two tests may be run at
# the same time without bumping heads without requiring the user to specify
# more than $TESTDIR.  See the test for leftover cvs-serv* directories near the
# end of this script at the end of "The big loop".
: ${TMPDIR=$TESTDIR/tmp}
export TMPDIR
if test -d $TMPDIR; then :; else
    mkdir $TMPDIR
fi

# Make sure various tools work the way we expect, or try to find
# versions that do.
: ${AWK=awk}
: ${EXPR=expr}
: ${ID=id}
: ${TR=tr}

# Keep track of tools that are found, but do NOT work as we hope
# in order to avoid them in future
badtools=
set_bad_tool ()
{
   badtools=$badtools:$1
}
is_bad_tool ()
{
   case ":$badtools:" in *:$1:*) return 0 ;; *) return 1 ; esac
}

version_test ()
{
  vercmd=$1
  verbad=:
  if RES=`$vercmd --version </dev/null 2>&1`; then
    if test "X$RES" != "X--version" && test "X$RES" != "X" ; then
      echo "$RES"
      verbad=false
    fi
  fi
  if $verbad; then
    echo "The command \`$vercmd' does not support the --version option."
  fi
  # It does not really matter that --version is not supported
  return 0
}

# Try to find a tool that satisfies all of the tests.
# Usage: list:of:colon:separated:alternatives test1 test2 test3 test4...
# Example: find_tool awk:gawk:nawk awk_tooltest1 awk_tooltest2
find_tool ()
{
  default_TOOL=$1
  echo find_tool: ${1+"$@"} >>$LOGFILE
  cmds="`IFS=:; echo $1`"; shift; tooltests="${1+$@}"
  if test -z "$tooltests"; then tooltests=version_test; fi
  clist=; for cmd in $cmds; do clist="$clist `Which -a $cmd`"; done
  # Make sure the default tool is just the first real command name
  for default_TOOL in $clist `IFS=:; echo $default_TOOL`; do break; done
  TOOL=""
  for trytool in $clist ; do
    pass=:
    for tooltest in $tooltests; do
      result=`eval $tooltest $trytool`
      rc=$?
      echo "Running $tooltest $trytool" >>$LOGFILE
      if test -n "$result"; then
	echo "$result" >>$LOGFILE
      fi
      if test "$rc" = "0"; then
        echo "PASS: $tooltest $trytool" >>$LOGFILE
      elif test "$rc" = "77"; then
        echo "MARGINAL: $tooltest $trytool; rc=$rc" >>$LOGFILE
        TOOL=$trytool
	pass=false
      else
        set_bad_tool $trytool
        echo "FAIL: $tooltest $trytool; rc=$rc" >>$LOGFILE
	pass=false
      fi
    done
    if $pass; then
      echo $trytool
      return 0
    fi
  done
  if test -n "$TOOL"; then
    echo "Notice: The default version of \`$default_TOOL' is defective." >>$LOGFILE
    echo "using \`$TOOL' and hoping for the best." >>$LOGFILE
    echo "Notice: The default version of \`$default_TOOL' is defective." >&2
    echo "using \`$TOOL' and hoping for the best." >&2
    echo $TOOL
  else
    echo $default_TOOL
  fi
}  

id_tool_test ()
{
  id=$1
  if $id -u >/dev/null 2>&1 && $id -un >/dev/null 2>&1; then
    return 0
  else
    echo "Running these tests requires an \`id' program that understands the"
    echo "-u and -n flags.  Make sure that such an id (GNU, or many but not"
    echo "all vendor-supplied versions) is in your path."
    return 1
  fi
}

ID=`find_tool id version_test id_tool_test`
echo "Using ID=$ID" >>$LOGFILE

# You can't run CVS as root; print a nice error message here instead
# of somewhere later, after making a mess.
for pass in false :; do
  case "`$ID -u 2>/dev/null`" in
    "0")
      echo "Test suite does not work correctly when run as root" >&2
      exit 1
      ;;

    *)
      break
      ;;
  esac
done

# Cause NextStep 3.3 users to lose in a more graceful fashion.
expr_tooltest1 ()
{
expr=$1
if $expr 'abc
def' : 'abc
def' >/dev/null; then
  # good, it works
  return 0
else
  echo 'Running these tests requires an "expr" program that can handle'
  echo 'multi-line patterns.  Make sure that such an expr (GNU, or many but'
  echo 'not all vendor-supplied versions) is in your path.'
  return 1
fi
}

# Warn SunOS, SysVr3.2, etc., users that they may be partially losing
# if we can't find a GNU expr to ease their troubles...
expr_tooltest2 ()
{
expr=$1
if $expr 'a
b' : 'a
c' >/dev/null; then
  echo 'WARNING: you are using a version of expr that does not correctly'
  echo 'match multi-line patterns.  Some tests may spuriously pass or fail.'
  echo 'You may wish to make sure GNU expr is in your path.'
  return 1
else
  return 0
fi
}

expr_create_bar ()
{
echo 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ' >${TESTDIR}/foo
cat ${TESTDIR}/foo ${TESTDIR}/foo ${TESTDIR}/foo ${TESTDIR}/foo >${TESTDIR}/bar
cat ${TESTDIR}/bar ${TESTDIR}/bar ${TESTDIR}/bar ${TESTDIR}/bar >${TESTDIR}/foo
cat ${TESTDIR}/foo ${TESTDIR}/foo ${TESTDIR}/foo ${TESTDIR}/foo >${TESTDIR}/bar
rm -f ${TESTDIR}/foo
}

expr_tooltest3 ()
{
expr=$1
# More SunOS lossage...
test ! -f ${TESTDIR}/bar && expr_create_bar
if $expr "`cat ${TESTDIR}/bar`" : "`cat ${TESTDIR}/bar`" >/dev/null; then
  : good, it works
else
  echo 'WARNING: you are using a version of expr that does not correctly'
  echo 'match large patterns.  Some tests may spuriously pass or fail.'
  echo 'You may wish to make sure GNU expr is in your path.'
  return 1
fi
if $expr "`cat ${TESTDIR}/bar`x" : "`cat ${TESTDIR}/bar`y" >/dev/null; then
  echo 'WARNING: you are using a version of expr that does not correctly'
  echo 'match large patterns.  Some tests may spuriously pass or fail.'
  echo 'You may wish to make sure GNU expr is in your path.'
  return 1
fi
# good, it works
return 0
}

# That we should have to do this is total bogosity, but GNU expr
# version 1.9.4-1.12 uses the emacs definition of "$" instead of the unix
# (e.g. SunOS 4.1.3 expr) one.  Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
ENDANCHOR="$"
expr_set_ENDANCHOR ()
{
expr=$1
ENDANCHOR="$"
if $expr 'abc
def' : 'abc$' >/dev/null; then
  ENDANCHOR='\'\'
   echo "Notice: An ENDANCHOR of dollar does not work."
   echo "Using a workaround for GNU expr versions 1.9.4 thru 1.12"
fi
return 0
}

# Work around another GNU expr (version 1.10-1.12) bug/incompatibility.
# "." doesn't appear to match a newline (it does with SunOS 4.1.3 expr).
# Note that the workaround is not a complete equivalent of .* because
# the first parenthesized expression in the regexp must match something
# in order for expr to return a successful exit status.
# Rumor has it this will be fixed in the
# next release of GNU expr after 1.12 (but we still have to cater to the old
# ones for some time because they are in many linux distributions).
DOTSTAR='.*'
expr_set_DOTSTAR ()
{
expr=$1
DOTSTAR='.*'
if $expr 'abc
def' : "a${DOTSTAR}f" >/dev/null; then
  : good, it works
else
  DOTSTAR='\(.\|
\)*'
  echo "Notice: DOTSTAR changed from sane \`.*' value to \`$DOTSTAR\`"
  echo "to workaround GNU expr version 1.10 thru 1.12 bug where \`.'"
  echo "does not match a newline."
fi
return 0
}

# Now that we have DOTSTAR, make sure it works with big matches
expr_tooltest_DOTSTAR ()
{
expr=$1
test ! -f ${TESTDIR}/bar && expr_create_bar
if $expr "`cat ${TESTDIR}/bar`" : "${DOTSTAR}xyzABC${DOTSTAR}$" >/dev/null; then
  # good, it works
  return 0
else
  echo 'WARNING: you are using a version of expr that does not correctly'
  echo 'match large patterns.  Some tests may spuriously pass or fail.'
  echo 'You may wish to make sure GNU expr is in your path.'
  return 77
fi
}

# FreeBSD 5.2 and 6.1 support 'expr [-e] expression' 
# They get confused unless '--' is used before the expressions
# when those expressions begin with a '-' character, such as the
# output of an ls -l command. The EXPR_COMPAT environment variable may
# be used to go back to the non-POSIX behavior as an alternative.
# (GNU expr appears to accept the '--' argument and work correctly or
# not have it and still get the results we want.)
exprDASHDASH='false'
expr_set_DASHDASH ()
{
expr=$1
exprDASHDASH='false'
# Not POSIX, but works on a lot of expr versions.
if $expr "-rw-rw-r--" : "-rw-rw-r--" >/dev/null 2>&1; then
  # good, it works
  return 0
else
  # Do things in the POSIX manner.
  if $expr -- "-rw-rw-r--" : "-rw-rw-r--" >/dev/null 2>&1; then
    exprDASHDASH=':'
    return 0
  else
    echo 'WARNING: Your $expr does not correctly handle'
    echo 'leading "-" characters in regular expressions to'
    echo 'be matched. You may wish to see if there is an'
    echo 'environment variable or other setting to allow'
    echo 'POSIX functionality to be enabled.'
    return 77
  fi
fi
}


EXPR=`find_tool ${EXPR}:gexpr \
  version_test expr_tooltest1 expr_tooltest2 expr_tooltest3 \
expr_set_ENDANCHOR expr_set_DOTSTAR expr_tooltest_DOTSTAR`

# Set the ENDANCHOR and DOTSTAR for the chosen expr version.
expr_set_ENDANCHOR ${EXPR} >/dev/null
expr_tooltest_DOTSTAR ${EXPR} >/dev/null

# Is $EXPR a POSIX or non-POSIX implementation
# with regard to command-line arguments?
expr_set_DASHDASH ${EXPR}
$exprDASHDASH && EXPR="$EXPR --"

echo "Using EXPR=$EXPR" >>$LOGFILE
echo "Using ENDANCHOR=$ENDANCHOR" >>$LOGFILE
echo "Using DOTSTAR=$DOTSTAR" >>$LOGFILE

# Cleanup
rm -f ${TESTDIR}/bar

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

# Now test the username to make sure it contains only valid characters
username=`$ID -un`
if $EXPR "${username}" : "${username}" >/dev/null; then
  : good, it works
else
  echo "Test suite does not work correctly when run by a username" >&2
  echo "containing regular expression meta-characters." >&2
  exit 1
fi

# Only 8 characters of $username appear in some output.
if test `echo $username |wc -c` -gt 8; then
  username8=`echo $username |sed 's/^\(........\).*/\1/'`
else
  username8=$username
fi

# Rarely, we need to match any username, not just the name of the user
# running this test.
#
# I'm not really sure what characters should be here.  a-zA-Z obviously.
# People complained when 0-9 were not allowed in usernames.  Other than that
# I'm not sure.
anyusername="[-a-zA-Z0-9][-a-zA-Z0-9]*"

# now make sure that tr works on NULs
tr_tooltest1 ()
{
tr=$1
if $EXPR `echo "123" | $tr '2' '\0'` : "123" >/dev/null 2>&1; then
  echo 'Warning: you are using a version of tr which does not correctly'
  echo 'handle NUL bytes.  Some tests may spuriously pass or fail.'
  echo 'You may wish to make sure GNU tr is in your path.'
  return 77
fi
# good, it works
return 0
}

TR=`find_tool ${TR}:gtr version_test tr_tooltest1`
echo "Using TR=$TR" >>$LOGFILE

# Awk testing

awk_tooltest1 ()
{
awk=$1
$awk 'BEGIN {printf("one\ntwo\nthree\nfour\nfive\nsix")}' </dev/null >abc
if $EXPR "`cat abc`" : \
'one
two
three
four
five
six'; then
  rm abc
  return 0
else
  rm abc
  echo "Notice: awk BEGIN clause or printf is not be working properly."
  return 1
fi
}

# Format item %c check
awk_tooltest2 ()
{
awk=$1
$awk 'BEGIN { printf "%c%c%c", 2, 3, 4 }' </dev/null \
  | ${TR} '\002\003\004' '123' >abc
if $EXPR "`cat abc`" : "123" ; then
  : good, found it
else
  echo "Notice: awk format %c string may not be working properly."
  rm abc
  return 77
fi
rm abc
return 0
}

AWK=`find_tool gawk:nawk:awk version_test awk_tooltest1 awk_tooltest2`
echo "Using AWK=$AWK" >>$LOGFILE

# Test that $1 works as a remote shell.  If so, set $host, $CVS_RSH, &
# $save_CVS_RSH to match and return 0.  Otherwise, set $skipreason and return
# 77.
depends_on_rsh ()
{
  host=${remotehost-"`hostname`"}
  result=`$1 $host 'echo test'`
  rc=$?
  if test $? != 0 || test "x$result" != "xtest"; then
    skipreason="\`$1 $host' failed rc=$rc result=$result"
    return 77
  fi

  save_CVS_RSH=$CVS_RSH
  CVS_RSH=$1; export CVS_RSH
  return 0
}

# Find a usable SSH.  When a usable ssh is found, set $host, $CVS_RSH, and
# $save_CVS_RSH and return 0.  Otherwise, set $skipreason and return 77.
depends_on_ssh ()
{
  case "$CVS_RSH" in
    *ssh*|*putty*)
      tryssh=`Which $CVS_RSH`
      if [ ! -n "$tryssh" ]; then
	skipreason="Unable to find CVS_RSH=$CVS_RSH executable"
	return 77
      elif [ ! -x "$tryssh" ]; then
	skipreason="Unable to execute $tryssh program"
	return 77
      fi
      ;;
    *)
      # Look in the user's PATH for "ssh"
      tryssh=`Which ssh`
      if test ! -r "$tryssh"; then
	skipreason="Unable to find ssh program"
	return 77
      fi
      ;;
  esac

  depends_on_rsh "$tryssh"
  return $?
}

pass ()
{
  echo "PASS: $1" >>${LOGFILE}
  passed=`expr $passed + 1`
}

# Like skip(), but don't fail when $skipfail is set.
skip_always ()
{
  echo "SKIP: $1${2+ ($2)}" >>$LOGFILE
  skipped=`expr $skipped + 1`
}

skip ()
{
  if $skipfail; then
    fail "$1${2+ ($2)}"
  else
    echo "SKIP: $1${2+ ($2)}" >>$LOGFILE
  fi
  skipped=`expr $skipped + 1`
}

warn ()
{
  if $skipfail; then
    fail "$1${2+ ($2)}"
  else
    echo "WARNING: $1${2+ ($2)}" >>$LOGFILE
  fi
  warnings=`expr $warnings + 1`
}

# Convenience function for skipping tests run only in local mode.
localonly ()
{
  skip_always $1 "only tested in local mode"
}

fail ()
{
  echo "FAIL: $1" | tee -a ${LOGFILE}
  echo "*** Please see the \`TESTS' and \`check.log' files for more information." >&2
  # This way the tester can go and see what remnants were left
  exit 1
}

verify_tmp_empty ()
{
  # Test our temp directory for cvs-serv* directories and cvsXXXXXX temp
  # files.  We would like to not leave any behind.
  if $remote && ls $TMPDIR/cvs-serv* >/dev/null 2>&1; then
    # A true value means ls found files/directories with these names.
    # Give the server some time to finish, then retry.
    sleep 1
    if ls $TMPDIR/cvs-serv* >/dev/null 2>&1; then
      warn "$1" "Found cvs-serv* directories in $TMPDIR."
      # The above will exit if $skipfail
      rm -rf $TMPDIR/cvs-serv*
    fi
  fi
  if ls $TMPDIR/cvs?????? >/dev/null 2>&1; then
    # A true value means ls found files/directories with these names.
    warn "$1" "Found cvsXXXXXX temp files in $TMPDIR."
    # The above will exit if $skipfail
    rm -f ls $TMPDIR/cvs??????
  fi
}

# Restore changes to CVSROOT admin files.
restore_adm ()
{
    rm -rf $CVSROOT_DIRNAME/CVSROOT
    cp -Rp $TESTDIR/CVSROOT.save $CVSROOT_DIRNAME/CVSROOT
}

# See dotest and dotest_fail for explanation (this is the parts
# of the implementation common to the two).
dotest_internal ()
{
  if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : "$3${ENDANCHOR}" >/dev/null; then
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
    verify_tmp_empty "$1"
  # expr can't distinguish between "zero characters matched" and "no match",
  # so special-case it.
  elif test -z "$3" && test ! -s ${TESTDIR}/dotest.tmp; then
    pass "$1"
    verify_tmp_empty "$1"
  elif test x"$4" != x; then
    if $EXPR "`cat ${TESTDIR}/dotest.tmp`" : "$4${ENDANCHOR}" >/dev/null; then
      cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
      pass "$1"
      verify_tmp_empty "$1"
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
  while [ $line -le `wc -l <${TESTDIR}/dotest.tmp` ] ; do
    if $EXPR "`sed -n ${line}p ${TESTDIR}/dotest.tmp`" : \
       "`sed -n ${line}p ${TESTDIR}/dotest.exp`" >/dev/null; then
      :
    elif test -z "`sed -n ${line}p ${TESTDIR}/dotest.tmp`" &&
       test -z "`sed -n ${line}p ${TESTDIR}/dotest.exp`"; then
      :
    else
      echo "Line $line:" >> ${LOGFILE}
      echo "**** expected: " >>${LOGFILE}
      sed -n ${line}p ${TESTDIR}/dotest.exp >>${LOGFILE}
      echo "**** got: " >>${LOGFILE}
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
      verify_tmp_empty "$1"
    fi
  else
    echo "$3" > ${TESTDIR}/dotest.exp
    if dotest_line_by_line "$1" "$2"; then
      pass "$1"
      verify_tmp_empty "$1"
    else
      if test x"$4" != x; then
	mv ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.ex1
	echo "$4" > ${TESTDIR}/dotest.exp
	if dotest_line_by_line "$1" "$2"; then
	  pass "$1"
	  verify_tmp_empty "$1"
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
  eval "$2" >${TESTDIR}/dotest.tmp 2>&1
  status=$?
  if test "$status" != 0; then
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
  eval "$2" >${TESTDIR}/dotest.tmp 2>&1
  status=$?
  if test "$status" != 0; then
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  cat >${TESTDIR}/dotest.exp
  if cmp ${TESTDIR}/dotest.exp ${TESTDIR}/dotest.tmp >/dev/null 2>&1; then
    pass "$1"
    verify_tmp_empty "$1"
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
  eval "$2" >${TESTDIR}/dotest.tmp 2>&1
  status=$?
  if test "$status" = 0; then
    cat ${TESTDIR}/dotest.tmp >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  dotest_internal "$@"
}

# Like dotest except output is sorted.
dotest_sort ()
{
  rm -f ${TESTDIR}/dotest.ex? 2>&1
  eval "$2" >${TESTDIR}/dotest.tmp1 2>&1
  status=$?
  if test "$status" != 0; then
    cat ${TESTDIR}/dotest.tmp1 >>${LOGFILE}
    echo "exit status was $status" >>${LOGFILE}
    fail "$1"
  fi
  ${TR} '	' ' ' < ${TESTDIR}/dotest.tmp1 | sort > ${TESTDIR}/dotest.tmp
  dotest_internal "$@"
}

# A function for fetching the timestamp of a revison of a file
getrlogdate () {
    ${testcvs} -n rlog -N ${1+"$@"} |
    while read token value; do
	case "$token" in
	date:)
	    echo $value | sed "s,;.*,,"
	    break;
            ;;
	esac
    done
}

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
	# Basic/miscellaneous functionality
	tests="version basica basicb basicc basic1 deep basic2"
	tests="${tests} parseroot parseroot2 files spacefiles commit-readonly"
	tests="${tests} commit-add-missing"
	tests="$tests add-restricted"
	tests="${tests} status"
	# Branching, tagging, removing, adding, multiple directories
	tests="${tests} rdiff rdiff-short"
	tests="${tests} rdiff2 diff diffnl death death2 death-rtag"
	tests="${tests} rm-update-message rmadd rmadd2 rmadd3 resurrection"
	tests="${tests} dirs dirs2 branches branches2 tagc tagf "
	tests="${tests} tag-log tag-space"
	tests="${tests} rcslib multibranch import importb importc import-CVS"
	tests="$tests import-quirks"
	tests="${tests} update-p import-after-initial branch-after-import"
	tests="${tests} join join2 join3 join4 join5 join6 join7 join8 join9"
	tests="${tests} join-readonly-conflict join-admin join-admin-2"
	tests="${tests} join-rm"
	tests="${tests} new newb conflicts conflicts2 conflicts3 conflicts4"
	tests="${tests} clean"
	# Checking out various places (modules, checkout -d, &c)
	tests="${tests} modules modules2 modules3 modules4 modules5 modules6"
	tests="${tests} modules7 mkmodules co-d"
	tests="${tests} cvsadm emptydir abspath abspath2 toplevel toplevel2"
        tests="${tests} rstar-toplevel trailingslashes checkout_repository"
	# Log messages, error messages.
	tests="${tests} mflag editor errmsg1 errmsg2 adderrmsg opterrmsg"
	# Watches, binary files, history browsing, &c.
	tests="${tests} devcom devcom2 devcom3 watch4 watch5 watch6"
	tests="${tests} unedit-without-baserev"
	tests="${tests} ignore ignore-on-branch binfiles binfiles2 binfiles3"
	tests="${tests} mcopy binwrap binwrap2"
	tests="${tests} binwrap3 mwrap info taginfo config"
	tests="${tests} serverpatch log log2 logopt ann ann-id"
	# Repository Storage (RCS file format, CVS lock files, creating
	# a repository without "cvs init", &c).
	tests="${tests} crerepos crerepos-extssh rcs rcs2 rcs3 rcs4 rcs5 rcs6"
	tests="$tests lockfiles backuprecover"
	tests="${tests} sshstdio"
	# More history browsing, &c.
	tests="${tests} history"
	tests="${tests} big modes modes2 modes3 stamps"
	# PreservePermissions stuff: permissions, symlinks et al.
	# tests="${tests} perms symlinks symlinks2 hardlinks"
	# More tag and branch tests, keywords.
	tests="${tests} sticky keyword keywordlog keywordname keyword2"
	tests="${tests} head tagdate multibranch2 tag8k"
	# "cvs admin", reserved checkouts.
	tests="${tests} admin reserved"
	# Nuts and bolts of diffing/merging (diff library, &c)
	tests="${tests} diffmerge1 diffmerge2"
	# Release of multiple directories
	tests="${tests} release"
	tests="${tests} recase"
	# Multiple root directories and low-level protocol tests.
	tests="${tests} multiroot multiroot2 multiroot3 multiroot4"
	tests="$tests rmroot reposmv pserver server server2 server3"
	tests="$tests client client2"
	tests="${tests} dottedroot fork commit-d"
else
	tests="$*"
fi

# Now check the -f argument for validity.
if test -n "$fromtest"; then
	# Don't allow spaces - they are our delimiters in tests
	count=0
	for sub in $fromtest; do
	  count=`expr $count + 1`
	done
	if test $count != 1; then
		echo "No such test \`$fromtest'." >&2
		exit 2
	fi
	# make sure it is in $tests
	case " $tests " in
		*" $fromtest "*)
			;;
		*)
			echo "No such test \`$fromtest'." >&2
			exit 2
			;;
	esac
fi



# a simple function to compare directory contents
#
# Returns: 0 for same, 1 for different
#
directory_cmp ()
{
	OLDPWD=`pwd`
	DIR_1=$1
	DIR_2=$2

	cd $DIR_1
	find . -print | fgrep -v /CVS | sort > $TESTDIR/dc$$d1

	# go back where we were to avoid symlink hell...
	cd $OLDPWD
	cd $DIR_2
	find . -print | fgrep -v /CVS | sort > $TESTDIR/dc$$d2

	if diff $TESTDIR/dc$$d1 $TESTDIR/dc$$d2 >/dev/null 2>&1
	then
		:
	else
		return 1
	fi
	cd $OLDPWD
	while read a
	do
		if test -f $DIR_1/"$a" ; then
			cmp -s $DIR_1/"$a" $DIR_2/"$a"
			if test $? -ne 0 ; then
				return 1
			fi
		fi
	done < $TESTDIR/dc$$d1
	rm -f $TESTDIR/dc$$*
	return 0
}



#
# The following 4 functions are used by the diffmerge1 test case.  They set up,
# respectively, the four versions of the files necessary:
#
#	1.  Ancestor revisions.
#	2.  "Your" changes.
#	3.  "My" changes.
#	4.  Expected merge result.
#

# Create ancestor revisions for diffmerge1
diffmerge_create_older_files() {
	  # This test case was supplied by Noah Friedman:
	  cat >testcase01 <<EOF
// Button.java

package random.application;

import random.util.*;

public class Button
{
  /* Instantiates a Button with origin (0, 0) and zero width and height.
   * You must call an initializer method to properly initialize the Button.
   */
  public Button ()
  {
    super ();

    _titleColor = Color.black;
    _disabledTitleColor = Color.gray;
    _titleFont = Font.defaultFont ();
  }

  /* Convenience constructor for instantiating a Button with
   * bounds x, y, width, and height.  Equivalent to
   *     foo = new Button ();
   *     foo.init (x, y, width, height);
   */
  public Button (int x, int y, int width, int height)
  {
    this ();
    init (x, y, width, height);
  }
}
EOF

	  # This test case was supplied by Jacob Burckhardt:
	  cat >testcase02 <<EOF
a
a
a
a
a
EOF

	  # This test case was supplied by Karl Tomlinson who also wrote the
	  # patch which lets CVS correctly handle this and several other cases:
	  cat >testcase03 <<EOF
x
s
a
b
s
y
EOF

	  # This test case was supplied by Karl Tomlinson:
	  cat >testcase04 <<EOF
s
x
m
m
x
s
v
s
x
m
m
x
s
EOF

	  # This test case was supplied by Karl Tomlinson:
	  cat >testcase05 <<EOF
s
x
m
m
x
x
x
x
x
x
x
x
x
x
s
s
s
s
s
s
s
s
s
s
v
EOF

	  # This test case was supplied by Jacob Burckhardt:
	  cat >testcase06 <<EOF
g











i
EOF

	  # This test is supposed to verify that the horizon lines are the same
	  # for both 2-way diffs, but unfortunately, it does not fail with the
	  # old version of cvs.  However, Karl Tomlinson still thought it would
	  # be good to test it anyway:
	  cat >testcase07 <<EOF
h
f









g
r



i










i
EOF

	  # This test case was supplied by Jacob Burckhardt:
	  cat >testcase08 <<EOF
Both changes move this line to the end of the file.

no
changes
here

First change will delete this line.

First change will also delete this line.

    no
    changes
    here

Second change will change it here.

        no
        changes
        here
EOF

	  # This test case was supplied by Jacob Burckhardt.  Note that I do not
	  # think cvs has ever failed with this case, but I include it anyway,
	  # since I think it is a hard case.  It is hard because Peter Miller's
	  # fmerge utility fails on it:
	  cat >testcase09 <<EOF
m
a
{
}
b
{
}
EOF

	  # This test case was supplied by Martin Dorey and simplified by Jacob
	  # Burckhardt:
	  cat >testcase10 <<EOF

    petRpY ( MtatRk );
    fV ( MtatRk != Zy ) UDTXUK_DUUZU ( BGKT_ZFDK_qjT_HGTG );

    MtatRk = MQfr_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_KRLIep * jfle_Uecopd_MfJe_fY_nEtek );
    OjZy MtatRk = Uead_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_MfJe_fY_nEtek, nRVVep );

    Bloke_GttpfIRte_MtpeaL ( &acI );
MTGTXM Uead_Ktz_qjT_jfle_Uecopd ( fYt Y, uofd *nRVVep )
{
    fV ( Y < 16 )
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                      Y * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }
    elke
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtqfppHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                 ( Y - 16 ) * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }

}


/****************************************************************************
*                                                                           *
*   Uead Mectopk ( Uelatfue to tze cRppeYt raptftfoY )                      *
*                                                                           *
****************************************************************************/

MTGTXM Uead_Mectopk ( RfYt64 Mtapt_Mectop, RfYt64 KRL_Mectopk, uofd *nRVVep )
{
MTGTXM MtatRk = Zy;

    MtatRk = Uead_HfkQ ( FaptftfoY_TaIle.Uelatfue_Mectop + Mtapt_Mectop, KRL_Mectopk, nRVVep );

    petRpY ( MtatRk );

}
    HfkQipfte ( waYdle,                 /*  waYdle                         */
                waYdleFok,              /*  ZVVket VpoL ktapt oV dfkQ      */
                (coYkt RfYt8*) nRVVep,  /*  nRVVep                         */
                0,                      /*  MRrepVlRoRk KfxoYfkL           */
                beYgtz                  /*  nEtek to Apfte                 */
              );

    petRpY ( Zy );
}
EOF
}

# Create "your" revisions for diffmerge1
diffmerge_create_your_files() {
	  # remove the Button() method
	  cat >testcase01 <<\EOF
// Button.java

package random.application;

import random.util.*;

public class Button
{
  /* Instantiates a Button with origin (0, 0) and zero width and height.
   * You must call an initializer method to properly initialize the Button.
   */
  public Button ()
  {
    super ();

    _titleColor = Color.black;
    _disabledTitleColor = Color.gray;
    _titleFont = Font.defaultFont ();
  }
}
EOF

	  cat >testcase02 <<\EOF
y
a
a
a
a
EOF

	  cat >testcase03 <<\EOF
x
s
a
b
s
b
s
y
EOF

	  cat >testcase04 <<\EOF
s
m
s
v
s
m
s
EOF

	  cat >testcase05 <<\EOF
v
s
m
s
s
s
s
s
s
s
s
s
s
v
EOF

	  # Test case 6 and test case 7 both use the same input files, but they
	  # order the input files differently.  In one case, a certain file is
	  # used as the older file, but in the other test case, that same file
	  # is used as the file which has changes.  I could have put echo
	  # commands here, but since the echo lines would be the same as those
	  # in the previous function, I decided to save space and avoid repeating
	  # several lines of code.  Instead, I merely swap the files:
	  mv testcase07 tmp
	  mv testcase06 testcase07
	  mv tmp testcase06

	  # Make the date newer so that cvs thinks that the files are changed:
	  touch testcase06 testcase07

	  cat >testcase08 <<\EOF
no
changes
here

First change has now added this in.

    no
    changes
    here

Second change will change it here.

        no
        changes
        here

Both changes move this line to the end of the file.
EOF

	  cat >testcase09 <<\EOF

m
a
{
}
b
{
}
c
{
}
EOF

	  cat >testcase10 <<\EOF

    fV ( BzQkV_URYYfYg ) (*jfle_Uecopdk)[0].jfle_Uecopd_KRLIep = ZpfgfYal_jUK;

    petRpY ( MtatRk );
    fV ( MtatRk != Zy ) UDTXUK_DUUZU ( BGKT_ZFDK_qjT_HGTG );

    fV ( jfle_Uecopd_KRLIep < 16 )
    {
        MtatRk = Uead_Ktz_qjT_jfle_Uecopd ( jfle_Uecopd_KRLIep, (uofd*)nRVVep );
    }
    elke
    {
        MtatRk = ZreY_GttpfIRte_MtpeaL ( qjT_jfle_Uecopdk, qjT_jfle_Uecopd_BoRYt, HGTG_TvFD, KXbb, KXbb, &acI );
        fV ( MtatRk != Zy ) UDTXUK_DUUZU ( BGKT_ZFDK_qjT_HGTG );

        MtatRk = MQfr_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_KRLIep * jfle_Uecopd_MfJe_fY_nEtek );
        OjZy MtatRk = Uead_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_MfJe_fY_nEtek, nRVVep );

    Bloke_GttpfIRte_MtpeaL ( &acI );
MTGTXM Uead_Ktz_qjT_jfle_Uecopd ( fYt Y, uofd *nRVVep )
{
MTGTXM MtatRk = Zy;

    fV ( Y < 16 )
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                      Y * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }
    elke
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtqfppHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                 ( Y - 16 ) * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }

    petRpY ( MtatRk );

}


/****************************************************************************
*                                                                           *
*   Uead Mectopk ( Uelatfue to tze cRppeYt raptftfoY )                      *
*                                                                           *
****************************************************************************/

MTGTXM Uead_Mectopk ( RfYt64 Mtapt_Mectop, RfYt64 KRL_Mectopk, uofd *nRVVep )
{
MTGTXM MtatRk = Zy;

    MtatRk = Uead_HfkQ ( FaptftfoY_TaIle.Uelatfue_Mectop + Mtapt_Mectop, KRL_Mectopk, nRVVep );

    petRpY ( MtatRk );

}
    HfkQipfte ( waYdle,                 /*  waYdle                         */
                waYdleFok,              /*  ZVVket VpoL ktapt oV dfkQ      */
                (coYkt RfYt8*) nRVVep,  /*  nRVVep                         */
                0,                      /*  MRrepVlRoRk KfxoYfkL           */
                beYgtz                  /*  nEtek to Apfte                 */
              );

    petRpY ( Zy );
}

EOF
}

# Create "my" revisions for diffmerge1
diffmerge_create_my_files() {
          # My working copy still has the Button() method, but I
	  # comment out some code at the top of the class.
	  cat >testcase01 <<\EOF
// Button.java

package random.application;

import random.util.*;

public class Button
{
  /* Instantiates a Button with origin (0, 0) and zero width and height.
   * You must call an initializer method to properly initialize the Button.
   */
  public Button ()
  {
    super ();

    // _titleColor = Color.black;
    // _disabledTitleColor = Color.gray;
    // _titleFont = Font.defaultFont ();
  }

  /* Convenience constructor for instantiating a Button with
   * bounds x, y, width, and height.  Equivalent to
   *     foo = new Button ();
   *     foo.init (x, y, width, height);
   */
  public Button (int x, int y, int width, int height)
  {
    this ();
    init (x, y, width, height);
  }
}
EOF

	  cat >testcase02 <<\EOF
a
a
a
a
m
EOF

	  cat >testcase03 <<\EOF
x
s
c
s
b
s
y
EOF

	  cat >testcase04 <<\EOF
v
s
x
m
m
x
s
v
s
x
m
m
x
s
v
EOF

	  # Note that in test case 5, there are no changes in the "mine"
	  # section, which explains why there is no command here which writes to
	  # file testcase05.

	  # no changes for testcase06

	  # The two branches make the same changes:
	  cp ../yours/testcase07 .

	  cat >testcase08 <<\EOF
no
changes
here

First change will delete this line.

First change will also delete this line.

    no
    changes
    here

Second change has now changed it here.

        no
        changes
        here

Both changes move this line to the end of the file.
EOF

	  cat >testcase09 <<\EOF
m
a
{
}
b
{
}
c
{
}
EOF

	  cat >testcase10 <<\EOF

    petRpY ( MtatRk );
    fV ( MtatRk != Zy ) UDTXUK_DUUZU ( BGKT_ZFDK_qjT_HGTG );

    MtatRk = MQfr_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_KRLIep * jfle_Uecopd_MfJe_fY_nEtek );
    OjZy MtatRk = Uead_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_MfJe_fY_nEtek, nRVVep );

    Bloke_GttpfIRte_MtpeaL ( &acI );
MTGTXM Uead_Ktz_qjT_jfle_Uecopd ( fYt Y, uofd *nRVVep )
{
    fV ( Y < 16 )
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                      Y * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }
    elke
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtqfppHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                 ( Y - 16 ) * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }

}


/****************************************************************************
*                                                                           *
*   Uead Mectopk ( Uelatfue to tze cRppeYt raptftfoY )                      *
*                                                                           *
****************************************************************************/

MTGTXM Uead_Mectopk ( RfYt64 Mtapt_Mectop, RfYt64 KRL_Mectopk, uofd *nRVVep )
{
MTGTXM MtatRk = Zy;

    MtatRk = Uead_HfkQ ( FaptftfoY_TaIle.Uelatfue_Mectop + Mtapt_Mectop, KRL_Mectopk, nRVVep );

    petRpY ( MtatRk );

}
    HfkQipfte ( waYdle,                 /*  waYdle                         */
                waYdleFok,              /*  ZVVket VpoL ktapt oV dfkQ      */
                (coYkt RfYt8*) nRVVep,  /*  nRVVep                         */
                beYgtz                  /*  nEtek to Apfte                 */
              );

    petRpY ( Zy );
}

EOF
}

# Create expected results of merge for diffmerge1
diffmerge_create_expected_files() {
	  cat >testcase01 <<\EOF
// Button.java

package random.application;

import random.util.*;

public class Button
{
  /* Instantiates a Button with origin (0, 0) and zero width and height.
   * You must call an initializer method to properly initialize the Button.
   */
  public Button ()
  {
    super ();

    // _titleColor = Color.black;
    // _disabledTitleColor = Color.gray;
    // _titleFont = Font.defaultFont ();
  }
}
EOF

	  cat >testcase02 <<\EOF
y
a
a
a
m
EOF

	  cat >testcase03 <<\EOF
x
s
c
s
b
s
b
s
y
EOF

	  cat >testcase04 <<\EOF
v
s
m
s
v
s
m
s
v
EOF

	  # Since there are no changes in the "mine" section, just take exactly
	  # the version in the "yours" section:
	  cp ../yours/testcase05 .

	  cp ../yours/testcase06 .

	  # Since the two branches make the same changes, the result should be
	  # the same as both branches.  Here, I happen to pick yours to copy from,
	  # but I could have also picked mine, since the source of the copy is
	  # the same in either case.  However, the mine has already been
	  # altered by the update command, so don't use it.  Instead, use the
	  # yours section which has not had an update on it and so is unchanged:
	  cp ../yours/testcase07 .

	  cat >testcase08 <<\EOF
no
changes
here

First change has now added this in.

    no
    changes
    here

Second change has now changed it here.

        no
        changes
        here

Both changes move this line to the end of the file.
EOF

	  cat >testcase09 <<\EOF

m
a
{
}
b
{
}
c
{
}
EOF

	  cat >testcase10 <<\EOF

    fV ( BzQkV_URYYfYg ) (*jfle_Uecopdk)[0].jfle_Uecopd_KRLIep = ZpfgfYal_jUK;

    petRpY ( MtatRk );
    fV ( MtatRk != Zy ) UDTXUK_DUUZU ( BGKT_ZFDK_qjT_HGTG );

    fV ( jfle_Uecopd_KRLIep < 16 )
    {
        MtatRk = Uead_Ktz_qjT_jfle_Uecopd ( jfle_Uecopd_KRLIep, (uofd*)nRVVep );
    }
    elke
    {
        MtatRk = ZreY_GttpfIRte_MtpeaL ( qjT_jfle_Uecopdk, qjT_jfle_Uecopd_BoRYt, HGTG_TvFD, KXbb, KXbb, &acI );
        fV ( MtatRk != Zy ) UDTXUK_DUUZU ( BGKT_ZFDK_qjT_HGTG );

        MtatRk = MQfr_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_KRLIep * jfle_Uecopd_MfJe_fY_nEtek );
        OjZy MtatRk = Uead_GttpfIRte_MtpeaL ( &acI, jfle_Uecopd_MfJe_fY_nEtek, nRVVep );

    Bloke_GttpfIRte_MtpeaL ( &acI );
MTGTXM Uead_Ktz_qjT_jfle_Uecopd ( fYt Y, uofd *nRVVep )
{
MTGTXM MtatRk = Zy;

    fV ( Y < 16 )
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                      Y * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }
    elke
    {
        petRpY ( Uead_Mectopk ( noot_Uecopd.qVtqfppHatabcY0 * noot_Uecopd.MectopkFepBlRktep +
                                                 ( Y - 16 ) * jfle_Uecopd_MfJe_fY_Mectopk,
                                jfle_Uecopd_MfJe_fY_Mectopk,
                                nRVVep ) );
    }

    petRpY ( MtatRk );

}


/****************************************************************************
*                                                                           *
*   Uead Mectopk ( Uelatfue to tze cRppeYt raptftfoY )                      *
*                                                                           *
****************************************************************************/

MTGTXM Uead_Mectopk ( RfYt64 Mtapt_Mectop, RfYt64 KRL_Mectopk, uofd *nRVVep )
{
MTGTXM MtatRk = Zy;

    MtatRk = Uead_HfkQ ( FaptftfoY_TaIle.Uelatfue_Mectop + Mtapt_Mectop, KRL_Mectopk, nRVVep );

    petRpY ( MtatRk );

}
    HfkQipfte ( waYdle,                 /*  waYdle                         */
                waYdleFok,              /*  ZVVket VpoL ktapt oV dfkQ      */
                (coYkt RfYt8*) nRVVep,  /*  nRVVep                         */
                beYgtz                  /*  nEtek to Apfte                 */
              );

    petRpY ( Zy );
}

EOF
}



# Echo a new CVSROOT based on $1, $remote, and $remotehost
newroot() {
  if $remote; then
    if test -n "$remotehost"; then
      echo :ext:$remotehost$1
    else
      echo :fork:$1
    fi
  else
    echo $1
  fi
}



# Set up CVSROOT (the crerepos tests will test operating without CVSROOT set).
#
# Currently we test :fork: and :ext: (see crerepos test).  There is a
# known difference between the two in modes-15 (see comments there).
#
# :ext: can be tested against a remote machine if:
#
#    1. $remotehost is set using the `-h' option to this script.
#    2. ${CVS_RSH=rsh} $remotehost works.
#    3. The path to $TESTDIR is the same on both machines (symlinks are okay)
#    4. The path to $testcvs is the same on both machines (symlinks are okay)
#       or $CVS_SERVER is overridden in this script's environment to point to
#       a working CVS exectuable on the remote machine.
#
# Testing :pserver: would be hard (inetd issues).  (How about using tcpserver
# and some high port number?  DRP)

# Allow CVS_SERVER to be overridden.  This facilitates constructs like
# testing a local case-insensitive client against a remote case
# sensitive server and visa versa.
: ${CVS_SERVER=$testcvs}; export CVS_SERVER

# Use a name which will be different than CVSROOT on case insensitive
# filesystems (e.g., HFS+)
CVSROOTDIR=cvsrootdir
if $linkroot; then
    mkdir ${TESTDIR}/realcvsroot
    ln -s realcvsroot ${TESTDIR}/${CVSROOTDIR}
fi
CVSROOT_DIRNAME=${TESTDIR}/${CVSROOTDIR}
CVSROOT=`newroot $CVSROOT_DIRNAME`; export CVSROOT



###
### Init the repository.
###
dotest init-1 "$testcvs -d$CVSROOT_DIRNAME init"

# Copy the admin files for restore_adm.
cp -Rp $CVSROOT_DIRNAME/CVSROOT $TESTDIR/CVSROOT.save



###
### The tests
###
if $remote; then
	localonly init-2
	localonly init-3
else
	dotest init-2 "$testcvs init"
	dotest_fail init-3 "$testcvs -d $CVSROOT/sdir init" \
"$PROG \[init aborted\]: Cannot initialize repository under existing CVSROOT: \`$CVSROOT_DIRNAME'"
fi



### The big loop
for what in $tests; do
	if test -n "$fromtest" ; then
	    if test $fromtest = $what ; then
		unset fromtest
	    else
		continue
	    fi
	fi

	if $verbose; then
	    echo "$what:"
	fi

	case $what in

	version)
	  # We've had cases where the version command started dumping core,
	  # so we might as well test it
	  dotest version-1 "${testcvs} --version" \
'
Concurrent Versions System (CVS) [0-9.]*.*

Copyright (C) [0-9]* Free Software Foundation, Inc.

Senior active maintainers include Larry Jones, Derek R. Price,
and Mark D. Baushke.  Please see the AUTHORS and README files from the CVS
distribution kit for a complete list of contributors and copyrights.

CVS may be copied only under the terms of the GNU General Public License,
a copy of which can be found with the CVS distribution kit.

Specify the --help option for further information about CVS'

	  if $remote; then
		dotest version-2r "${testcvs} version" \
'Client: Concurrent Versions System (CVS) [0-9p.]* (client/server)
Server: Concurrent Versions System (CVS) [0-9p.]* (client/server)'
	  else
		dotest version-2 "${testcvs} version" \
'Concurrent Versions System (CVS) [0-9.]*.*'
	  fi
	  ;;

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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd ..
	  rm -r 1

	  dotest basica-1 "${testcvs} -q co first-dir" ''
	  cd first-dir

	  # Test a few operations, to ensure they gracefully do
	  # nothing in an empty directory.
	  dotest basica-1a0 "${testcvs} -q update" ''
	  dotest basica-1a1 "${testcvs} -q diff -c" ''
	  dotest basica-1a2 "${testcvs} -q status" ''
	  dotest basica-1a3 "${testcvs} -q update ." ''
	  dotest basica-1a4 "${testcvs} -q update ./" ''

	  mkdir sdir
	  # Remote CVS gives the "cannot open CVS/Entries" error, which is
	  # clearly a bug, but not a simple one to fix.
	  dotest basica-1a10 "${testcvs} -n add sdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir added to the repository" \
"${PROG} add: cannot open CVS/Entries for reading: No such file or directory
Directory ${CVSROOT_DIRNAME}/first-dir/sdir added to the repository"
	  dotest_fail basica-1a11 \
	    "test -d ${CVSROOT_DIRNAME}/first-dir/sdir" ''
	  dotest basica-2 "${testcvs} add sdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir added to the repository"
	  cd sdir
	  mkdir ssdir
	  dotest basica-3 "${testcvs} add ssdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir added to the repository"
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
"${PROG}"' add: scheduling file `ssfile'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest_fail basica-4a "${testcvs} tag tag0 ssfile" \
"${PROG} tag: nothing known about ssfile
${PROG} "'\[tag aborted\]: correct the above errors first!'
	  cd ../..
	  dotest basica-5 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
done
Checking in sdir/ssdir/ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
initial revision: 1\.1
done"
	  dotest_fail basica-5a \
	    "${testcvs} -q tag BASE sdir/ssdir/ssfile" \
"${PROG} tag: Attempt to add reserved tag name BASE
${PROG} \[tag aborted\]: failed to set tag BASE to revision 1\.1 in ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v"
	  dotest basica-5b "${testcvs} -q tag NOT_RESERVED" \
'T sdir/ssdir/ssfile'

	  dotest basica-6 "${testcvs} -q update" ''
	  echo "ssfile line 2" >>sdir/ssdir/ssfile
	  dotest_fail basica-6.2 "${testcvs} -q diff -c" \
"Index: sdir/ssdir/ssfile
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* sdir/ssdir/ssfile	${RFCDATE}	1\.1
--- sdir/ssdir/ssfile	${RFCDATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
${PLUS} ssfile line 2"
	  dotest_fail basica-6.3 "${testcvs} -q diff -c -rBASE" \
"Index: sdir/ssdir/ssfile
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -r1\.1 ssfile
\*\*\* sdir/ssdir/ssfile	${RFCDATE}	1\.1
--- sdir/ssdir/ssfile	${RFCDATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  ssfile
${PLUS} ssfile line 2"
	  dotest_fail basica-6.4 "${testcvs} -q diff -c -rBASE -C3isacrowd" \
"Index: sdir/ssdir/ssfile
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
retrieving revision 1\.1
diff -c -C 3isacrowd -r1\.1 ssfile
${PROG} diff: invalid context length argument"
	  dotest basica-7 "${testcvs} -q ci -m modify-it" \
"Checking in sdir/ssdir/ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest_fail basica-nonexist "${testcvs} -q ci nonexist" \
"${PROG}"' [a-z]*: nothing known about `nonexist'\''
'"${PROG}"' \[[a-z]* aborted\]: correct above errors first!'
	  dotest basica-8 "${testcvs} -q update ." ''

	  # Test the -f option to ci
	  cd sdir/ssdir
	  dotest basica-8a0 "${testcvs} -q ci -m not-modified ssfile" ''
	  dotest basica-8a "${testcvs} -q ci -f -m force-it" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest basica-8a1 "${testcvs} -q ci -m bump-it -r 2.0" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.0; previous revision: 1\.3
done"
	  dotest basica-8a1a "${testcvs} -q ci -m bump-it -r 2.9" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.9; previous revision: 2\.0
done"
	  # Test string-based revion number increment rollover
	  dotest basica-8a1b "${testcvs} -q ci -m bump-it -f -r 2" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.10; previous revision: 2\.9
done"
	  dotest basica-8a1c "${testcvs} -q ci -m bump-it -r 2.99" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.99; previous revision: 2\.10
done"
	  # Test string-based revion number increment rollover
	  dotest basica-8a1d "${testcvs} -q ci -m bump-it -f -r 2" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.100; previous revision: 2\.99
done"
	  dotest basica-8a1e "${testcvs} -q ci -m bump-it -r 2.1099" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.1099; previous revision: 2\.100
done"
	  # Test string-based revion number increment rollover
	  dotest basica-8a1f "${testcvs} -q ci -m bump-it -f -r 2" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 2\.1100; previous revision: 2\.1099
done"
	  # -f should not be necessary, but it should be harmless.
	  # Also test the "-r 3" (rather than "-r 3.0") usage.
	  dotest basica-8a2 "${testcvs} -q ci -m bump-it -f -r 3" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 3\.1; previous revision: 2\.1100
done"

	  # Test using -r to create a branch
	  dotest_fail basica-8a3 "${testcvs} -q ci -m bogus -r 3.0.0" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
${PROG} commit: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v: can't find branch point 3\.0
${PROG} commit: could not check in ssfile"
	  dotest basica-8a4 "${testcvs} -q ci -m valid -r 3.1.2" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 3\.1\.2\.1; previous revision: 3\.1
done"

	  # Verify that this file remains unchanged since up -A should not
	  # change the contents here.
	  cp ssfile $TESTDIR/ssfile.sav
	  # now get rid of the sticky tag and go back to the trunk
	  dotest basica-8a5 "$testcvs -q up -A ./" '[UP] ssfile'
	  dotest basica-8a6 "cmp ssfile $TESTDIR/ssfile.sav"
	  rm $TESTDIR/ssfile.sav

	  cd ../..
	  dotest basica-8b "${testcvs} -q diff -r1.2 -r1.3"
	  dotest basica-8b1 "${testcvs} -q diff -r1.2 -r1.3 -C 3isacrowd"

	  # The .* here will normally be "No such file or directory",
	  # but if memory serves some systems (AIX?) have a different message.
:	  dotest_fail basica-9 \
	    "${testcvs} -q -d ${TESTDIR}/nonexist update" \
"${PROG}: cannot access cvs root ${TESTDIR}/nonexist: .*"
	  dotest_fail basica-9 \
	    "${testcvs} -q -d ${TESTDIR}/nonexist update" \
"${PROG} \[[a-z]* aborted\]: ${TESTDIR}/nonexist/CVSROOT: .*"

	  dotest basica-10 "${testcvs} annotate" \
'
Annotations for sdir/ssdir/ssfile
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          .'"$username8"' *[0-9a-zA-Z-]*.: ssfile
1\.2          .'"$username8"' *[0-9a-zA-Z-]*.: ssfile line 2'

	  # Test resurrecting with strange revision numbers
	  cd sdir/ssdir
	  dotest basica-r1 "${testcvs} rm -f ssfile" \
"${PROG} remove: scheduling .ssfile. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest basica-r2 "${testcvs} -q ci -m remove" \
"Removing ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: delete; previous revision: 3\.1
done"
	  dotest basica-r3 "${testcvs} -q up -p -r 3.1 ./ssfile >ssfile" ""
	  dotest basica-r4 "${testcvs} add ssfile" \
"${PROG} add: Re-adding file .ssfile. (in place of dead revision 3\.2)\.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest basica-r5 "${testcvs} -q ci -m resurrect" \
"Checking in ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v  <--  ssfile
new revision: 3\.3; previous revision: 3\.2
done"
	  cd ../..

	  # As long as we have a file with a few revisions, test
	  # a few "cvs admin -o" invocations.
	  cd sdir/ssdir
	  dotest_fail basica-o1 "${testcvs} admin -o 1.2::1.2" \
"${PROG} [a-z]*: while processing more than one file:
${PROG} \[[a-z]* aborted\]: attempt to specify a numeric revision"
	  dotest basica-o2 "${testcvs} admin -o 1.2::1.2 ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o2a "${testcvs} admin -o 1.1::NOT_RESERVED ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest_fail basica-o2b "${testcvs} admin -o 1.1::NOT_EXIST ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v: Revision NOT_EXIST doesn't exist.
${PROG} admin: RCS file for .ssfile. not modified\."
	  dotest basica-o3 "${testcvs} admin -o 1.2::1.3 ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o4 "${testcvs} admin -o 3.1:: ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
deleting revision 3\.3
deleting revision 3\.2
done"
	  dotest basica-o5 "${testcvs} admin -o ::1.1 ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
done"
	  dotest basica-o5a "${testcvs} -n admin -o 1.2::3.1 ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
deleting revision 2\.1100
deleting revision 2\.1099
deleting revision 2\.100
deleting revision 2\.99
deleting revision 2\.10
deleting revision 2\.9
deleting revision 2\.0
deleting revision 1\.3
done"
	  dotest basica-o6 "${testcvs} admin -o 1.2::3.1 ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
deleting revision 2\.1100
deleting revision 2\.1099
deleting revision 2\.100
deleting revision 2\.99
deleting revision 2\.10
deleting revision 2\.9
deleting revision 2\.0
deleting revision 1\.3
done"
	  dotest basica-o6a "${testcvs} admin -o 3.1.2: ssfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
deleting revision 3\.1\.2\.1
done"
	  dotest basica-o7 "${testcvs} log -N ssfile" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/ssdir/ssfile,v
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
	  dotest basica-o8 "${testcvs} -q update -p -r 1.1 ./ssfile" "ssfile"
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
"${PROG} add: scheduling file .topfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest basicb-0c "${testcvs} -q ci -m add-it topfile" \
"RCS file: ${CVSROOT_DIRNAME}/topfile,v
done
Checking in topfile;
${CVSROOT_DIRNAME}/topfile,v  <--  topfile
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd ..
	  rm -r 2

	  dotest basicb-1 "${testcvs} -q co first-dir" ''

	  # The top-level CVS directory is not created by default.
	  # I'm leaving basicb-1a and basicb-1b untouched, mostly, in
	  # case we decide that the default should be reversed...

	  dotest_fail basicb-1a "test -d CVS" ''

	  dotest basicb-1c "cat first-dir/CVS/Repository" "first-dir"

	  cd first-dir
	  # Note that the name Emptydir is chosen to test that CVS just
	  # treats it like any other directory name.  It should be
	  # special only when it is directly in $CVSROOT/CVSROOT.
	  mkdir Emptydir sdir2
	  dotest basicb-2 "${testcvs} add Emptydir sdir2" \
"Directory ${CVSROOT_DIRNAME}/first-dir/Emptydir added to the repository
Directory ${CVSROOT_DIRNAME}/first-dir/sdir2 added to the repository"
	  cd Emptydir
	  echo sfile1 starts >sfile1
	  dotest basicb-2a10 "${testcvs} -n add sfile1" \
"${PROG} add: scheduling file .sfile1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest basicb-2a11 "${testcvs} status sfile1" \
"${PROG} status: use .${PROG} add. to create an entry for sfile1
===================================================================
File: sfile1           	Status: Unknown

   Working revision:	No entry for sfile1
   Repository revision:	No revision control file"
	  dotest basicb-3 "${testcvs} add sfile1" \
"${PROG} add: scheduling file .sfile1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
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
"${PROG} add: scheduling file .sfile2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest basicb-4a "${testcvs} -q ci CVS" \
"${PROG} [a-z]*: warning: directory CVS specified in argument
${PROG} [a-z]*: but CVS uses CVS for its own purposes; skipping CVS directory"
	  cd ..
	  dotest basicb-5 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Emptydir/sfile1,v
done
Checking in Emptydir/sfile1;
${CVSROOT_DIRNAME}/first-dir/Emptydir/sfile1,v  <--  sfile1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir2/sfile2,v
done
Checking in sdir2/sfile2;
${CVSROOT_DIRNAME}/first-dir/sdir2/sfile2,v  <--  sfile2
initial revision: 1\.1
done"
	  echo sfile1 develops >Emptydir/sfile1
	  dotest basicb-6 "${testcvs} -q ci -m modify" \
"Checking in Emptydir/sfile1;
${CVSROOT_DIRNAME}/first-dir/Emptydir/sfile1,v  <--  sfile1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest basicb-7 "${testcvs} -q tag release-1" 'T Emptydir/sfile1
T sdir2/sfile2'
	  echo not in time for release-1 >sdir2/sfile2
	  dotest basicb-8 "${testcvs} -q ci -m modify-2" \
"Checking in sdir2/sfile2;
${CVSROOT_DIRNAME}/first-dir/sdir2/sfile2,v  <--  sfile2
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

	  # basicb-9a and basicb-9b: see note about basicb-1a

	  dotest_fail basicb-9a "test -d CVS" ''

	  dotest basicb-9c "cat newdir/CVS/Repository" "\."
	  dotest basicb-9d "cat newdir/first-dir/CVS/Repository" \
"${CVSROOT_DIRNAME}/first-dir" \
"first-dir"
	  dotest basicb-9e "cat newdir/first-dir/Emptydir/CVS/Repository" \
"${CVSROOT_DIRNAME}/first-dir/Emptydir" \
"first-dir/Emptydir"
	  dotest basicb-9f "cat newdir/first-dir/sdir2/CVS/Repository" \
"${CVSROOT_DIRNAME}/first-dir/sdir2" \
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
	  dotest basicb-12 "${testcvs} -q update ./." ''
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
"Directory ${CVSROOT_DIRNAME}/second-dir added to the repository"
	  cd second-dir
	  touch aa
	  dotest basicb-16 "${testcvs} add aa" \
"${PROG} add: scheduling file .aa. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest basicb-17 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/second-dir/aa,v
done
Checking in aa;
${CVSROOT_DIRNAME}/second-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  cd ..

	  # Try to remove all revisions in a file.
	  dotest_fail basicb-o1 "${testcvs} admin -o1.1 topfile" \
"RCS file: ${CVSROOT_DIRNAME}/topfile,v
deleting revision 1\.1
${PROG} \[admin aborted\]: attempt to delete all revisions"
	  dotest basicb-o2 "${testcvs} -q update -d first-dir" \
"U first-dir/Emptydir/sfile1
U first-dir/sdir2/sfile2"
	  dotest_fail basicb-o3 \
"${testcvs} admin -o1.1:1.2 first-dir/sdir2/sfile2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir2/sfile2,v
deleting revision 1\.2
deleting revision 1\.1
${PROG} \[admin aborted\]: attempt to delete all revisions"
	  cd ..
	  rm -r 1

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

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -rf ${CVSROOT_DIRNAME}/second-dir
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository
Directory ${CVSROOT_DIRNAME}/second-dir added to the repository"
	  # Old versions of CVS often didn't create this top-level CVS
	  # directory in the first place.  I think that maybe the only
	  # way to get it to work currently is to let CVS create it,
	  # and then blow it away (don't complain if it does not
	  # exist).  But that is perfectly legal; people who are used
	  # to the old behavior especially may be interested.
	  # FIXME: this test is intended for the TopLevelAdmin=yes case;
	  # should adjust/move it accordingly.
	  rm -rf CVS
	  dotest basicc-4 "echo *" "first-dir second-dir"
	  dotest basicc-5 "${testcvs} update" \
"${PROG} update: Updating first-dir
${PROG} update: Updating second-dir" \
"${PROG} update: Updating \.
${PROG} update: Updating first-dir
${PROG} update: Updating second-dir"

	  cd first-dir
	  dotest basicc-6 "${testcvs} release -d" ""
	  dotest basicc-7 "test -d ../first-dir" ""
	  # The Linux 2.2 kernel lets you delete ".".  That's OK either way,
	  # the point is that CVS must not mess with anything *outside* "."
	  # the way that CVS 1.10 and older tried to.
	  dotest basicc-8 "${testcvs} -Q release -d ." \
"" "${PROG} release: deletion of directory \. failed: .*"
	  dotest basicc-9 "test -d ../second-dir" ""
	  # For CVS to make a syntactic check for "." wouldn't suffice.
	  # On Linux 2.2 systems, the cwd may be gone, so we recreate it
          # to allow basicc-11 to actually happen 
	  if test ! -d ../first-dir; then
	    # Apparently `cd ..' doesn't work with Linux 2.2 & Bash 2.05b.
	    cd $TESTDIR/1
	    mkdir ./first-dir
            cd ./first-dir
	  fi
	  dotest basicc-11 "${testcvs} -Q release -d ./." \
"" "${PROG} release: deletion of directory \./\. failed: .*"
	  dotest basicc-11a "test -d ../second-dir" ""

	  cd ../..

	  mkdir 2; cd 2
	  dotest basicc-12 "${testcvs} -Q co ." ""
	  # actual entries can be in either Entries or Entries.log, do
	  # an update to get them consolidated into Entries
	  dotest basicc-12a "${testcvs} -Q up" ""
	  dotest basicc-12b "cat CVS/Entries" \
"D/CVSROOT////
D/first-dir////
D/second-dir////"
	  dotest basicc-13 "echo *" "CVS CVSROOT first-dir second-dir"
	  dotest basicc-14 "${testcvs} -Q release first-dir second-dir" ""
	  # a normal release shouldn't affect the Entries file
	  dotest basicc-14b "cat CVS/Entries" \
"D/CVSROOT////
D/first-dir////
D/second-dir////"
	  # FIXCVS: but release -d probably should
	  dotest basicc-15 "${testcvs} -Q release -d first-dir second-dir" ""
	  dotest basicc-16 "echo *" "CVS CVSROOT"
	  dotest basicc-17 "cat CVS/Entries" \
"D/CVSROOT////
D/first-dir////
D/second-dir////"
	  # FIXCVS: if not, update should notice the missing directories
	  # and update Entries accordingly
	  dotest basicc-18 "${testcvs} -Q up" ""
	  dotest basicc-19 "cat CVS/Entries" \
"D/CVSROOT////
D/first-dir////
D/second-dir////"

	  cd ..
	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	basic1)
	  # first dive - add a files, first singly, then in a group.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir basic1; cd basic1
	  # check out an empty directory
	  dotest basic1-1 "${testcvs} -q co first-dir" ''

	  cd first-dir
	  echo file2 >file2
	  echo file3 >file3
	  echo file4 >file4
	  echo file5 >file5

	  dotest basic1-14-add-add "${testcvs} add file2 file3 file4 file5" \
"${PROG} add: scheduling file \`file2' for addition
${PROG} add: scheduling file \`file3' for addition
${PROG} add: scheduling file \`file4' for addition
${PROG} add: scheduling file \`file5' for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
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
"${PROG} log: file2 has been added, but not committed
${PROG} log: file3 has been added, but not committed
${PROG} log: file4 has been added, but not committed
${PROG} log: file5 has been added, but not committed"
	  cd ..
	  dotest basic1-21-add-add "${testcvs} -q update" \
"A first-dir/file2
A first-dir/file3
A first-dir/file4
A first-dir/file5"
	  # FIXCVS?  Shouldn't this read first-dir/file2 instead of file2?
	  dotest basic1-22-add-add "${testcvs} log first-dir" \
"${PROG} log: Logging first-dir
${PROG} log: file2 has been added, but not committed
${PROG} log: file3 has been added, but not committed
${PROG} log: file4 has been added, but not committed
${PROG} log: file5 has been added, but not committed"
	  dotest basic1-23-add-add "${testcvs} status first-dir" \
"${PROG} status: Examining first-dir
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
"${PROG} update: Updating first-dir
A first-dir/file2
A first-dir/file3
A first-dir/file4
A first-dir/file5"
	  dotest basic1-27-add-add "${testcvs} co first-dir" \
"${PROG} checkout: Updating first-dir
A first-dir/file2
A first-dir/file3
A first-dir/file4
A first-dir/file5"
	  cd first-dir
	  dotest basic1-14-add-ci \
"${testcvs} commit -m test file2 file3 file4 file5" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file5,v
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
initial revision: 1\.1
done"
	  dotest basic1-15-add-ci \
"${testcvs} -q update file2 file3 file4 file5" ''
	  dotest basic1-16-add-ci "${testcvs} -q update" ''
	  dotest basic1-17-add-ci "${testcvs} -q status" \
"===================================================================
File: file2            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file3,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file4            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file4,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file5            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file5,v
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
"${PROG} remove: scheduling .file2. for removal
${PROG} remove: scheduling .file3. for removal
${PROG} remove: scheduling .file4. for removal
${PROG} remove: scheduling .file5. for removal
${PROG} remove: use .${PROG} commit. to remove these files permanently"
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
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file3		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file3,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file4		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file4,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file5		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file5,v
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
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done
Removing file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
new revision: delete; previous revision: 1\.1
done
Removing file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1
done
Removing file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
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

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r basic1
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
"Directory ${CVSROOT_DIRNAME}/first-dir/dir1[/dir0-9]* added to the repository"
	    cd $i
	    echo file1 >file1
	    dotest deep-3-$i "${testcvs} add file1" \
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  done
	  cd ../../../../../../../../..
	  dotest_lit deep-4 "${testcvs} -q ci -m add-them first-dir" <<HERE
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/file1,v
done
Checking in first-dir/dir1/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file1,v
done
Checking in first-dir/dir1/dir2/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/file1,v
done
Checking in first-dir/dir1/dir2/dir3/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v
done
Checking in first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
initial revision: 1.1
done
HERE

	  cd first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8
	  rm file1
	  dotest deep-4a0 "${testcvs} rm file1" \
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest deep-4a1 "${testcvs} -q ci -m rm-it" "Removing file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/dir8/file1,v  <--  file1
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
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  cd ..
	  dotest deep-rm2 "${testcvs} -q update -d -P" 'R dir7/file1'
	  dotest deep-rm3 "test -d dir7" ''
	  dotest deep-rm4 "${testcvs} -q ci -m rm-it" "Removing dir7/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/dir7/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  dotest deep-rm5 "${testcvs} -q update -d -P" ''
	  dotest_fail deep-rm6 "test -d dir7" ''

	  # Test rm -f -R.
	  cd ../..
	  dotest deep-rm7 "${testcvs} rm -f -R dir5" \
"${PROG} remove: Removing dir5
${PROG} remove: scheduling .dir5/file1. for removal
${PROG} remove: Removing dir5/dir6
${PROG} remove: scheduling .dir5/dir6/file1. for removal
${PROG} remove: use .${PROG} commit. to remove these files permanently"
	  dotest deep-rm8 "${testcvs} -q ci -m rm-it" \
"Removing dir5/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done
Removing dir5/dir6/file1;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/dir3/dir4/dir5/dir6/file1,v  <--  file1
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
				dotest basic2-2-$i "${testcvs} add $i" \
"Directory ${CVSROOT_DIRNAME}/.*/$i added to the repository"
			fi

			cd $i

			for j in file6 file7; do
				echo $j > $j
			done

			dotest basic2-3-$i "${testcvs} add file6 file7" \
"${PROG} add: scheduling file .file6. for addition
${PROG} add: scheduling file .file7. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"

		done
		cd ../../..
		dotest basic2-4 "${testcvs} update first-dir" \
"${PROG} update: Updating first-dir
A first-dir/file6
A first-dir/file7
${PROG} update: Updating first-dir/dir1
A first-dir/dir1/file6
A first-dir/dir1/file7
${PROG} update: Updating first-dir/dir1/dir2
A first-dir/dir1/dir2/file6
A first-dir/dir1/dir2/file7"

		# fixme: doesn't work right for added files.
		dotest basic2-5 "${testcvs} log first-dir" \
"${PROG} log: Logging first-dir
${PROG} log: file6 has been added, but not committed
${PROG} log: file7 has been added, but not committed
${PROG} log: Logging first-dir/dir1
${PROG} log: file6 has been added, but not committed
${PROG} log: file7 has been added, but not committed
${PROG} log: Logging first-dir/dir1/dir2
${PROG} log: file6 has been added, but not committed
${PROG} log: file7 has been added, but not committed"

		dotest basic2-6 "${testcvs} status first-dir" \
"${PROG} status: Examining first-dir
===================================================================
File: file6            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file7            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

${PROG} status: Examining first-dir/dir1
===================================================================
File: file6            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file7            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

${PROG} status: Examining first-dir/dir1/dir2
===================================================================
File: file6            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file7            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

# XXX why is this commented out???
#		if ${CVS} diff -u first-dir   >> ${LOGFILE} || test $? = 1 ; then
#		    pass 34
#		else
#		    fail 34
#		fi

		dotest basic2-8 "${testcvs} -q ci -m 'second dive' first-dir" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file6,v
done
Checking in first-dir/file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file7,v
done
Checking in first-dir/file7;
${CVSROOT_DIRNAME}/first-dir/file7,v  <--  file7
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/file6,v
done
Checking in first-dir/dir1/file6;
${CVSROOT_DIRNAME}/first-dir/dir1/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/file7,v
done
Checking in first-dir/dir1/file7;
${CVSROOT_DIRNAME}/first-dir/dir1/file7,v  <--  file7
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file6,v
done
Checking in first-dir/dir1/dir2/file6;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file7,v
done
Checking in first-dir/dir1/dir2/file7;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file7,v  <--  file7
initial revision: 1\.1
done"

		dotest basic2-9 "${testcvs} tag second-dive first-dir" \
"${PROG} tag: Tagging first-dir
T first-dir/file6
T first-dir/file7
${PROG} tag: Tagging first-dir/dir1
T first-dir/dir1/file6
T first-dir/dir1/file7
${PROG} tag: Tagging first-dir/dir1/dir2
T first-dir/dir1/dir2/file6
T first-dir/dir1/dir2/file7"

		# third dive - in bunch o' directories, add bunch o' files,
		# delete some, change some.

		for i in first-dir dir1 dir2 ; do
			cd $i

			# modify a file
			echo file6 >>file6

			# delete a file
			rm file7

			dotest basic2-10-$i "${testcvs} rm file7" \
"${PROG} remove: scheduling .file7. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"

			# and add a new file
			echo file14 >file14

			dotest basic2-11-$i "${testcvs} add file14" \
"${PROG} add: scheduling file .file14. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
		done

		cd ../../..
		dotest basic2-12 "${testcvs} update first-dir" \
"${PROG} update: Updating first-dir
A first-dir/file14
M first-dir/file6
R first-dir/file7
${PROG} update: Updating first-dir/dir1
A first-dir/dir1/file14
M first-dir/dir1/file6
R first-dir/dir1/file7
${PROG} update: Updating first-dir/dir1/dir2
A first-dir/dir1/dir2/file14
M first-dir/dir1/dir2/file6
R first-dir/dir1/dir2/file7"

		# FIXME: doesn't work right for added files
		dotest basic2-13 "${testcvs} log first-dir" \
"${PROG} log: Logging first-dir
${PROG} log: file14 has been added, but not committed

RCS file: ${CVSROOT_DIRNAME}/first-dir/file6,v
Working file: first-dir/file6
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	second-dive: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
second dive
=============================================================================

RCS file: ${CVSROOT_DIRNAME}/first-dir/file7,v
Working file: first-dir/file7
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	second-dive: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
second dive
=============================================================================
${PROG} log: Logging first-dir/dir1
${PROG} log: file14 has been added, but not committed

RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/file6,v
Working file: first-dir/dir1/file6
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	second-dive: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
second dive
=============================================================================

RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/file7,v
Working file: first-dir/dir1/file7
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	second-dive: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
second dive
=============================================================================
${PROG} log: Logging first-dir/dir1/dir2
${PROG} log: file14 has been added, but not committed

RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file6,v
Working file: first-dir/dir1/dir2/file6
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	second-dive: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
second dive
=============================================================================

RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file7,v
Working file: first-dir/dir1/dir2/file7
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	second-dive: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
second dive
============================================================================="

		dotest basic2-14 "${testcvs} status first-dir" \
"${PROG} status: Examining first-dir
===================================================================
File: file14           	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file6            	Status: Locally Modified

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file6,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file7		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file7,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

${PROG} status: Examining first-dir/dir1
===================================================================
File: file14           	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file6            	Status: Locally Modified

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/dir1/file6,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file7		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/dir1/file7,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

${PROG} status: Examining first-dir/dir1/dir2
===================================================================
File: file14           	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file6            	Status: Locally Modified

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file6,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: no file file7		Status: Locally Removed

   Working revision:	-1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file7,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

# XXX why is this commented out?
#		if ${CVS} diff -u first-dir  >> ${LOGFILE} || test $? = 1 ; then
#		    pass 42
#		else
#		    fail 42
#		fi

		dotest basic2-16 "${testcvs} ci -m 'third dive' first-dir" \
"${PROG} [a-z]*: Examining first-dir
${PROG} [a-z]*: Examining first-dir/dir1
${PROG} [a-z]*: Examining first-dir/dir1/dir2
RCS file: ${CVSROOT_DIRNAME}/first-dir/file14,v
done
Checking in first-dir/file14;
${CVSROOT_DIRNAME}/first-dir/file14,v  <--  file14
initial revision: 1\.1
done
Checking in first-dir/file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
new revision: 1\.2; previous revision: 1\.1
done
Removing first-dir/file7;
${CVSROOT_DIRNAME}/first-dir/file7,v  <--  file7
new revision: delete; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/file14,v
done
Checking in first-dir/dir1/file14;
${CVSROOT_DIRNAME}/first-dir/dir1/file14,v  <--  file14
initial revision: 1\.1
done
Checking in first-dir/dir1/file6;
${CVSROOT_DIRNAME}/first-dir/dir1/file6,v  <--  file6
new revision: 1\.2; previous revision: 1\.1
done
Removing first-dir/dir1/file7;
${CVSROOT_DIRNAME}/first-dir/dir1/file7,v  <--  file7
new revision: delete; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file14,v
done
Checking in first-dir/dir1/dir2/file14;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file14,v  <--  file14
initial revision: 1\.1
done
Checking in first-dir/dir1/dir2/file6;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file6,v  <--  file6
new revision: 1\.2; previous revision: 1\.1
done
Removing first-dir/dir1/dir2/file7;
${CVSROOT_DIRNAME}/first-dir/dir1/dir2/file7,v  <--  file7
new revision: delete; previous revision: 1\.1
done"
		dotest basic2-17 "${testcvs} -q update first-dir" ''

		dotest basic2-18 "${testcvs} tag third-dive first-dir" \
"${PROG} tag: Tagging first-dir
T first-dir/file14
T first-dir/file6
${PROG} tag: Tagging first-dir/dir1
T first-dir/dir1/file14
T first-dir/dir1/file6
${PROG} tag: Tagging first-dir/dir1/dir2
T first-dir/dir1/dir2/file14
T first-dir/dir1/dir2/file6"

		dotest basic2-19 "echo yes | ${testcvs} release -d first-dir" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .first-dir.: "

		# end of third dive
		dotest_fail basic2-20 "test -d first-dir" ""

		# now try some rtags

		# rtag HEADS
		dotest basic2-21 "${testcvs} rtag rtagged-by-head first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: Tagging first-dir/dir1
${PROG} rtag: Tagging first-dir/dir1/dir2"
		# The next test used to cause an assert failure
		# something like:
		# cvs: ./recurse.c:667: do_recursion: Assertion `repository != ((void *)0)' failed.
		dotest basic2-21b "${testcvs} co -p -r rtagged-by-head first-dir/file6" \
"===================================================================
Checking out first-dir/file6
RCS:  $CVSROOT_DIRNAME/first-dir/file6,v
VERS: 1\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
file6
file6"
		# tag by tag
		dotest basic2-22 "${testcvs} rtag -r rtagged-by-head rtagged-by-tag first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: Tagging first-dir/dir1
${PROG} rtag: Tagging first-dir/dir1/dir2"

		# tag by revision
		dotest basic2-23 "${testcvs} rtag -r1.1 rtagged-by-revision first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: Tagging first-dir/dir1
${PROG} rtag: Tagging first-dir/dir1/dir2"

		# rdiff by revision
		dotest basic2-24 "${testcvs} rdiff -r1.1 -rrtagged-by-head first-dir" \
"${PROG} rdiff: Diffing first-dir
Index: first-dir/file6
diff -c first-dir/file6:1\.1 first-dir/file6:1\.2
\*\*\* first-dir/file6:1\.1	${DATE}
--- first-dir/file6	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  file6
${PLUS} file6
Index: first-dir/file7
diff -c first-dir/file7:1\.1 first-dir/file7:removed
\*\*\* first-dir/file7:1.1	${DATE}
--- first-dir/file7	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- file7
--- 0 ----
${PROG} rdiff: Diffing first-dir/dir1
Index: first-dir/dir1/file6
diff -c first-dir/dir1/file6:1\.1 first-dir/dir1/file6:1\.2
\*\*\* first-dir/dir1/file6:1\.1	${DATE}
--- first-dir/dir1/file6	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  file6
${PLUS} file6
Index: first-dir/dir1/file7
diff -c first-dir/dir1/file7:1\.1 first-dir/dir1/file7:removed
\*\*\* first-dir/dir1/file7:1\.1	${DATE}
--- first-dir/dir1/file7	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- file7
--- 0 ----
${PROG} rdiff: Diffing first-dir/dir1/dir2
Index: first-dir/dir1/dir2/file6
diff -c first-dir/dir1/dir2/file6:1\.1 first-dir/dir1/dir2/file6:1\.2
\*\*\* first-dir/dir1/dir2/file6:1\.1	${DATE}
--- first-dir/dir1/dir2/file6	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  file6
${PLUS} file6
Index: first-dir/dir1/dir2/file7
diff -c first-dir/dir1/dir2/file7:1\.1 first-dir/dir1/dir2/file7:removed
\*\*\* first-dir/dir1/dir2/file7:1\.1	${DATE}
--- first-dir/dir1/dir2/file7	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- file7
--- 0 ----"
		dotest basic2-24a "${testcvs} rdiff -l -r1.1 -rrtagged-by-head first-dir" \
"${PROG} rdiff: Diffing first-dir
Index: first-dir/file6
diff -c first-dir/file6:1\.1 first-dir/file6:1\.2
\*\*\* first-dir/file6:1\.1	${DATE}
--- first-dir/file6	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
--- 1,2 ----
  file6
${PLUS} file6
Index: first-dir/file7
diff -c first-dir/file7:1\.1 first-dir/file7:removed
\*\*\* first-dir/file7:1.1	${DATE}
--- first-dir/file7	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- file7
--- 0 ----"
		# now export by rtagged-by-head and rtagged-by-tag and compare.
		dotest basic2-25 "${testcvs} export -r rtagged-by-head -d 1dir first-dir" \
"${PROG} export: Updating 1dir
U 1dir/file14
U 1dir/file6
${PROG} export: Updating 1dir/dir1
U 1dir/dir1/file14
U 1dir/dir1/file6
${PROG} export: Updating 1dir/dir1/dir2
U 1dir/dir1/dir2/file14
U 1dir/dir1/dir2/file6"
		dotest_fail basic2-25a "test -d 1dir/CVS"
		dotest_fail basic2-25b "test -d 1dir/dir1/CVS"
		dotest_fail basic2-25c "test -d 1dir/dir1/dir2/CVS"

		dotest basic2-26 "${testcvs} export -r rtagged-by-tag first-dir" \
"${PROG} export: Updating first-dir
U first-dir/file14
U first-dir/file6
${PROG} export: Updating first-dir/dir1
U first-dir/dir1/file14
U first-dir/dir1/file6
${PROG} export: Updating first-dir/dir1/dir2
U first-dir/dir1/dir2/file14
U first-dir/dir1/dir2/file6"
		dotest_fail basic2-26a "test -d first-dir/CVS"
		dotest_fail basic2-26b "test -d first-dir/dir1/CVS"
		dotest_fail basic2-26c "test -d first-dir/dir1/dir2/CVS"

		dotest basic2-27 "directory_cmp 1dir first-dir"
		rm -r 1dir first-dir

		# checkout by revision vs export by rtagged-by-revision and compare.
		mkdir export-dir
		dotest basic2-28 "${testcvs} export -rrtagged-by-revision -d export-dir first-dir" \
"${PROG} export: Updating export-dir
U export-dir/file14
U export-dir/file6
U export-dir/file7
${PROG} export: Updating export-dir/dir1
U export-dir/dir1/file14
U export-dir/dir1/file6
U export-dir/dir1/file7
${PROG} export: Updating export-dir/dir1/dir2
U export-dir/dir1/dir2/file14
U export-dir/dir1/dir2/file6
U export-dir/dir1/dir2/file7"
		dotest_fail basic2-28a "test -d export-dir/CVS"
		dotest_fail basic2-28b "test -d export-dir/dir1/CVS"
		dotest_fail basic2-28c "test -d export-dir/dir1/dir2/CVS"

		dotest basic2-29 "${testcvs} co -r1.1 first-dir" \
"${PROG} checkout: Updating first-dir
U first-dir/file14
U first-dir/file6
U first-dir/file7
${PROG} checkout: Updating first-dir/dir1
U first-dir/dir1/file14
U first-dir/dir1/file6
U first-dir/dir1/file7
${PROG} checkout: Updating first-dir/dir1/dir2
U first-dir/dir1/dir2/file14
U first-dir/dir1/dir2/file6
U first-dir/dir1/dir2/file7"

		# directory copies are done in an oblique way in order to avoid a bug in sun's tmp filesystem.
		mkdir first-dir.cpy ; (cd first-dir ; tar cf - . | (cd ../first-dir.cpy ; tar xf -))

		dotest basic2-30 "directory_cmp first-dir export-dir"

		# interrupt, while we've got a clean 1.1 here, let's import it
		# into a couple of other modules.
		cd export-dir
		dotest_sort basic2-31 "${testcvs} import -m first-import second-dir first-immigration immigration1 immigration1_0" \
"

N second-dir/dir1/dir2/file14
N second-dir/dir1/dir2/file6
N second-dir/dir1/dir2/file7
N second-dir/dir1/file14
N second-dir/dir1/file6
N second-dir/dir1/file7
N second-dir/file14
N second-dir/file6
N second-dir/file7
No conflicts created by this import
${PROG} import: Importing ${CVSROOT_DIRNAME}/second-dir/dir1
${PROG} import: Importing ${CVSROOT_DIRNAME}/second-dir/dir1/dir2"
		cd ..

		dotest basic2-32 "${testcvs} export -r HEAD second-dir" \
"${PROG} export: Updating second-dir
U second-dir/file14
U second-dir/file6
U second-dir/file7
${PROG} export: Updating second-dir/dir1
U second-dir/dir1/file14
U second-dir/dir1/file6
U second-dir/dir1/file7
${PROG} export: Updating second-dir/dir1/dir2
U second-dir/dir1/dir2/file14
U second-dir/dir1/dir2/file6
U second-dir/dir1/dir2/file7"

		dotest basic2-33 "directory_cmp first-dir second-dir"

		rm -r second-dir

		rm -r export-dir first-dir
		mkdir first-dir
		(cd first-dir.cpy ; tar cf - . | (cd ../first-dir ; tar xf -))

		# update the top, cancelling sticky tags, retag, update other copy, compare.
		cd first-dir
		dotest basic2-34 "${testcvs} update -A -l *file*" \
"[UP] file6
${PROG} update: file7 is no longer in the repository"

		# If we don't delete the tag first, cvs won't retag it.
		# This would appear to be a feature.
		dotest basic2-35 "${testcvs} tag -l -d rtagged-by-revision" \
"${PROG} tag: Untagging \.
D file14
D file6"
		dotest basic2-36 "${testcvs} tag -l rtagged-by-revision" \
"${PROG} tag: Tagging \.
T file14
T file6"

		cd ..
		mv first-dir 1dir
		mv first-dir.cpy first-dir
		cd first-dir

		dotest basic2-37 "${testcvs} -q diff -u" ''

		dotest basic2-38 "${testcvs} update" \
"${PROG} update: Updating .
${PROG} update: Updating dir1
${PROG} update: Updating dir1/dir2"

		cd ..

		#### FIXME: is this expected to work???  Need to investigate
		#### and fix or remove the test.
#		dotest basic2-39 "directory_cmp 1dir first-dir"

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
		dotest basic2-64 "${testcvs} his -x TOFWUPCGMAR -a" \
"O [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir           =first-dir= ${TESTDIR}/\*
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir           == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir           == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1      == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1      == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1/dir2 == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1/dir2 == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir           == ${TESTDIR}
M [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == ${TESTDIR}
R [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir           == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1      == ${TESTDIR}
M [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1      == ${TESTDIR}
R [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir/dir1      == ${TESTDIR}
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1/dir2 == ${TESTDIR}
M [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1/dir2 == ${TESTDIR}
R [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir/dir1/dir2 == ${TESTDIR}
F [0-9-]* [0-9:]* ${PLUS}0000 ${username}                     =first-dir= ${TESTDIR}/\*
T [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-head:A\]
T [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-revision:1\.1\]
O [0-9-]* [0-9:]* ${PLUS}0000 ${username} \[1\.1\] first-dir           =first-dir= ${TESTDIR}/\*
U [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == ${TESTDIR}/first-dir
W [0-9-]* [0-9:]* ${PLUS}0000 ${username}     file7     first-dir           == ${TESTDIR}/first-dir" \
"O [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir           =first-dir= <remote>/\*
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir           == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir           == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1      == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1      == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file6     first-dir/dir1/dir2 == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file7     first-dir/dir1/dir2 == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir           == <remote>
M [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == <remote>
R [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir           == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1      == <remote>
M [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1      == <remote>
R [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir/dir1      == <remote>
A [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.1 file14    first-dir/dir1/dir2 == <remote>
M [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir/dir1/dir2 == <remote>
R [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file7     first-dir/dir1/dir2 == <remote>
F [0-9-]* [0-9:]* ${PLUS}0000 ${username}                     =first-dir= <remote>/\*
T [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-head:A\]
T [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-tag:rtagged-by-head\]
T [0-9-]* [0-9:]* ${PLUS}0000 ${username} first-dir \[rtagged-by-revision:1\.1\]
O [0-9-]* [0-9:]* ${PLUS}0000 ${username} \[1\.1\] first-dir           =first-dir= <remote>/\*
P [0-9-]* [0-9:]* ${PLUS}0000 ${username} 1\.2 file6     first-dir           == <remote>
W [0-9-]* [0-9:]* ${PLUS}0000 ${username}     file7     first-dir           == <remote>"

		rm -rf ${CVSROOT_DIRNAME}/first-dir
		rm -rf ${CVSROOT_DIRNAME}/second-dir
		;;

	parseroot)
	  mkdir 1; cd 1
	  # Test odd cases involving CVSROOT.  At the moment, that means we
	  # are testing roots with '/'s on the end, which CVS should parse off.
	  CVSROOT_save=${CVSROOT}
	  CVSROOT="${CVSROOT}/////"
	  dotest parseroot-1 "${testcvs} -q co CVSROOT/modules" \
"U CVSROOT/modules"
	  dotest parseroot-2 "${testcvs} -q ci -fmnull-change CVSROOT/modules" \
"Checking in CVSROOT/modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.2; previous revision: 1\.1
done
${PROG} commit: Rebuilding administrative file database"

	  if $remote; then
	    # I only test these when testing remote in case CVS was compiled
	    # without client support.

	    # logout does not try to contact the server.
	    CVSROOT=":pserver;proxy=localhost;proxyport=8080:localhost/dev/null"
	    dotest parseroot-3r "$testcvs -d'$CVSROOT' logout" \
"$PROG logout: WARNING: Ignoring method options found in CVSROOT: \`proxy=localhost;proxyport=8080'\.
$PROG logout: Use CVS version 1\.12\.7 or later to handle method options\.
Logging out of :pserver:$username@localhost:2401/dev/null
$PROG logout: warning: failed to open $HOME/\.cvspass for reading: No such file or directory
$PROG logout: Entry not found\."
	  fi

	  if $keep; then
		echo Keeping $TESTDIR and exiting due to --keep
		exit 0
	  fi

	  CVSROOT=$CVSROOT_save
	  cd ..
	  rm -r 1
	  ;;



	files)
	  # Test of how we specify files on the command line
	  # (recurse.c and that sort of thing).  Vaguely similar to
	  # tests like basic* and deep.  See modules and such tests
	  # for what happens when we throw in modules and co -d, &c.

	  # This particular test is fairly carefully crafted, to spot
	  # one particular issue with remote.
	  mkdir 1; cd 1
	  dotest files-1 "${testcvs} -q co -l ." ""
	  mkdir first-dir
	  dotest files-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch tfile
	  dotest files-3 "${testcvs} add tfile" \
"${PROG} add: scheduling file .tfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest files-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/tfile,v
done
Checking in tfile;
${CVSROOT_DIRNAME}/first-dir/tfile,v  <--  tfile
initial revision: 1\.1
done"
	  dotest files-5 "${testcvs} -q tag -b C" "T tfile"
	  dotest files-6 "$testcvs -q update -r C" "U tfile"
	  mkdir dir
	  dotest files-7 "${testcvs} add dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir added to the repository
--> Using per-directory sticky tag .C'"
	  cd dir
	  touch .file
	  dotest files-6 "${testcvs} add .file" \
"${PROG} add: scheduling file .\.file' for addition on branch .C.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  mkdir sdir
	  dotest files-7 "${testcvs} add sdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir/sdir added to the repository
--> Using per-directory sticky tag .C'"
	  cd sdir
	  mkdir ssdir
	  dotest files-8 "${testcvs} add ssdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir added to the repository
--> Using per-directory sticky tag .C'"
	  cd ssdir
	  touch .file
	  dotest files-9 "${testcvs} add .file" \
"${PROG} add: scheduling file .\.file' for addition on branch .C.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  cd ../..
	  dotest files-10 "${testcvs} -q ci -m test" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v
done
Checking in \.file;
${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir/Attic/\.file,v
done
Checking in sdir/ssdir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest files-11 \
"${testcvs} commit -m test -f ./.file ./sdir/ssdir/.file" \
"Checking in \.file;
${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done
Checking in \./sdir/ssdir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"
	  if $remote; then
	    # FIXCVS:
	    # This is a bug, looks like that toplevel_repos cruft in
	    # client.c is coming back to haunt us.
	    # May want to think about the whole issue, toplevel_repos
	    # has always been crufty and trying to patch it up again
	    # might be a mistake.
	    dotest files-12 \
"${testcvs} commit -f -m test ./sdir/ssdir/.file ./.file" \
"Checking in \./sdir/ssdir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.3; previous revision: 1\.1\.2\.2
done"

	    # Sync up the version numbers so that the rest of the
	    # tests don't need to expect different numbers based
	    # local or remote.
	    dotest files-12-workaround \
"${testcvs} commit -f -m test .file" \
"Checking in \.file;
${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.3; previous revision: 1\.1\.2\.2
done"
	  else
	    dotest files-12 \
"${testcvs} commit -f -m test ./sdir/ssdir/.file ./.file" \
"Checking in \./sdir/ssdir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.3; previous revision: 1\.1\.2\.2
done
Checking in \.file;
${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.3; previous revision: 1\.1\.2\.2
done"
	  fi
	  dotest files-13 \
"${testcvs} commit -fmtest ./sdir/../sdir/ssdir/..///ssdir/.file" \
"Checking in \./sdir/\.\./sdir/ssdir/\.\.///ssdir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/sdir/ssdir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.4; previous revision: 1\.1\.2\.3
done"
	  if $remote; then
	    dotest files-14 \
"${testcvs} commit -fmtest ../../first-dir/dir/.file" \
"Checking in \.\./\.\./first-dir/dir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v  <--  .file
new revision: 1\.1\.2\.4; previous revision: 1\.1\.2\.3
done"
	  else
	    dotest files-14 \
"${testcvs} commit -fmtest ../../first-dir/dir/.file" \
"Checking in \.\./\.\./first-dir/dir/\.file;
${CVSROOT_DIRNAME}/first-dir/dir/Attic/\.file,v  <--  \.file
new revision: 1\.1\.2\.4; previous revision: 1\.1\.2\.3
done"
	  fi
	  cd ../../..

	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	spacefiles)
	  # More filename tests, in particular spaces in file names.
	  # (it might be better to just change a few of the names in
	  # basica or some other test instead, always good to keep the
	  # testsuite concise).

	  mkdir 1; cd 1
	  dotest spacefiles-1 "${testcvs} -q co -l ." ""
	  touch ./-c
	  dotest spacefiles-2 "${testcvs} add -- -c" \
"${PROG} add: scheduling file .-c. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest spacefiles-3 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/-c,v
done
Checking in -c;
${CVSROOT_DIRNAME}/-c,v  <--  -c
initial revision: 1\.1
done"
	  mkdir 'first dir'
	  dotest spacefiles-4 "${testcvs} add 'first dir'" \
"Directory ${CVSROOT_DIRNAME}/first dir added to the repository"
	  mkdir ./-b
	  dotest spacefiles-5 "${testcvs} add -- -b" \
"Directory ${CVSROOT_DIRNAME}/-b added to the repository"
	  cd 'first dir'
	  touch 'a file'
	  dotest spacefiles-6 "${testcvs} add 'a file'" \
"${PROG} add: scheduling file .a file. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest spacefiles-7 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first dir/a file,v
done
Checking in a file;
${CVSROOT_DIRNAME}/first dir/a file,v  <--  a file
initial revision: 1\.1
done"
	  dotest spacefiles-8 "${testcvs} -q tag new-tag" "T a file"
	  cd ../..

	  mkdir 2; cd 2
	  dotest spacefiles-10 "${testcvs} co -- -b" \
"${PROG} checkout: Updating -b"
	  dotest spacefiles-11 "${testcvs} -q co -- -c" "U \./-c"
	  rm ./-c
	  dotest spacefiles-13 "${testcvs} -q co 'first dir'" \
"U first dir/a file"
	  cd ..

	  mkdir 3; cd 3
	  dotest spacefiles-14 "${testcvs} -q co 'first dir/a file'" \
"U first dir/a file"
	  cd ..

	  rm -r 1 2 3
	  rm -rf "${CVSROOT_DIRNAME}/first dir"
	  rm -r ${CVSROOT_DIRNAME}/-b
	  rm -f ${CVSROOT_DIRNAME}/-c,v
	  ;;

	commit-readonly)
	  mkdir 1; cd 1
	  module=x

	  : > junk
	  dotest commit-readonly-1 "$testcvs -Q import -m . $module X Y" ''
	  dotest commit-readonly-2 "$testcvs -Q co $module" ''
	  cd $module

	  file=m

	  # Include an rcs keyword to be expanded.
	  echo '$Id''$' > $file

	  dotest commit-readonly-3 "$testcvs add $file" \
"${PROG} add: scheduling file .$file. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest commit-readonly-4 "$testcvs -Q ci -m . $file" \
"RCS file: ${CVSROOT_DIRNAME}/$module/$file,v
done
Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
initial revision: 1\.1
done"

	  echo line2 >> $file
	  # Make the file read-only.
	  chmod a-w $file

	  dotest commit-readonly-5 "$testcvs -Q ci -m . $file" \
"Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
new revision: 1\.2; previous revision: 1\.1
done"

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	status)
		# This tests for a bug in the status command which failed to
		# notice resolved conflicts.
		mkdir status; cd status
		dotest status-init-1 "${testcvs} -q co -l ." ""
		mkdir first-dir
		dotest status-init-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
		cd first-dir
		echo a line >tfile
		dotest status-init-3 "${testcvs} add tfile" \
"${PROG} add: scheduling file .tfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
		dotest status-init-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/tfile,v
done
Checking in tfile;
${CVSROOT_DIRNAME}/first-dir/tfile,v  <--  tfile
initial revision: 1\.1
done"
		cd ..
		dotest status-init-5 "${testcvs} -q co -dsecond-dir first-dir" \
"U second-dir/tfile"
		cd second-dir
		echo some junk >>tfile
		dotest status-init-6 "${testcvs} -q ci -maline" \
"Checking in tfile;
${CVSROOT_DIRNAME}/first-dir/tfile,v  <--  tfile
new revision: 1\.2; previous revision: 1\.1
done"
		cd ../first-dir
		echo force a conflict >>tfile
		dotest status-init-7 "${testcvs} -q up" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/tfile,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into tfile
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in tfile
C tfile"

		# Now note our status
		dotest status-1 "${testcvs} status tfile" \
"===================================================================
File: tfile            	Status: Unresolved Conflict

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/tfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

		# touch the file, leaving conflict markers in place
		# and note our status
		touch tfile
		dotest status-2 "${testcvs} status tfile" \
"===================================================================
File: tfile            	Status: File had conflicts on merge

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/tfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

		# resolve the conflict
		echo resolution >tfile
		dotest status-3 "${testcvs} status tfile" \
"===================================================================
File: tfile            	Status: Locally Modified

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/tfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

		# Check that there are no problems just using CVS/Root too.
		save_CVSROOT=$CVSROOT
		unset CVSROOT
		dotest status-3a "${testcvs} status tfile" \
"===================================================================
File: tfile            	Status: Locally Modified

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/tfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		CVSROOT=$save_CVSROOT
		export CVSROOT

		# FIXCVS:
		# Update is supposed to re-Register() the file when it
		# finds resolved conflicts:
		dotest status-4 "grep 'Result of merge' CVS/Entries" \
"/tfile/1\.2/Result of merge${PLUS}[a-zA-Z0-9 :]*//"

                cd ..
                mkdir fourth-dir
                dotest status-init-8 "$testcvs add fourth-dir" \
"Directory $CVSROOT_DIRNAME/fourth-dir added to the repository"
                cd fourth-dir
                echo yet another line >t3file
                dotest status-init-9 "$testcvs add t3file" \
"$PROG add: scheduling file .t3file. for addition
$PROG add: use .$PROG commit. to add this file permanently"
                dotest status-init-10 "$testcvs -q ci -m add" \
"RCS file: $CVSROOT_DIRNAME/fourth-dir/t3file,v
done
Checking in t3file;
$CVSROOT_DIRNAME/fourth-dir/t3file,v  <--  t3file
initial revision: 1\.1
done"
                cd ../first-dir
                mkdir third-dir
                dotest status-init-11 "$testcvs add third-dir" \
"Directory $CVSROOT_DIRNAME/first-dir/third-dir added to the repository"
                cd third-dir
                echo another line >t2file
                dotest status-init-12 "$testcvs add t2file" \
"$PROG add: scheduling file .t2file. for addition
$PROG add: use .$PROG commit. to add this file permanently"
                dotest status-init-13 "$testcvs -q ci -m add" \
"RCS file: $CVSROOT_DIRNAME/first-dir/third-dir/t2file,v
done
Checking in t2file;
$CVSROOT_DIRNAME/first-dir/third-dir/t2file,v  <--  t2file
initial revision: 1\.1
done"
                dotest status-5 "$testcvs status ../tfile" \
"===================================================================
File: tfile            	Status: Locally Modified

   Working revision:	1\.2.*
   Repository revision:	1\.2	$CVSROOT_DIRNAME/first-dir/tfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
                dotest status-6 "$testcvs status ../../fourth-dir/t3file" \
"===================================================================
File: t3file           	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/fourth-dir/t3file,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

		if $keep; then
			echo Keeping $TESTDIR and exiting due to --keep
			exit 0
		fi

		cd ../../..
		rm -rf status
		rm -rf $CVSROOT_DIRNAME/first-dir $CVSROOT_DIRNAME/fourth-dir
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
"${PROG} checkout: Updating trdiff
U trdiff/bar
U trdiff/foo"
		cd trdiff
		echo something >> foo
		dotest rdiff-3 \
		  "${testcvs} ci -m added-something foo" \
"Checking in foo;
${CVSROOT_DIRNAME}/trdiff/foo,v  <--  foo
new revision: 1\.2; previous revision: 1\.1
done"
		echo '#ident	"@(#)trdiff:$''Name$:$''Id$"' > new
		echo "new file" >> new
		dotest rdiff-4 \
		  "${testcvs} add -m new-file-description new" \
"${PROG} add: scheduling file \`new' for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
		dotest rdiff-5 \
		  "${testcvs} commit -m added-new-file new" \
"RCS file: ${CVSROOT_DIRNAME}/trdiff/new,v
done
Checking in new;
${CVSROOT_DIRNAME}/trdiff/new,v  <--  new
initial revision: 1\.1
done"
		dotest rdiff-6 \
		  "${testcvs} tag local-v0" \
"${PROG} tag: Tagging .
T bar
T foo
T new"
		dotest rdiff-7 \
		  "${testcvs} status -v foo" \
"===================================================================
File: foo              	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/trdiff/foo,v
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
"${PROG}"' rdiff: Diffing trdiff
Index: trdiff/foo
diff -c trdiff/foo:1\.1\.1\.1 trdiff/foo:1\.2
\*\*\* trdiff/foo:1\.1\.1\.1	'"${DATE}"'
--- trdiff/foo	'"${DATE}"'
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
\*\*\* /dev/null	'"${DATE}"'
--- trdiff/new	'"${DATE}"'
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1,2 ----
'"${PLUS}"' #ident	"@(#)trdiff:\$''Name: local-v0 \$:\$''Id: new,v 1\.1 [0-9/]* [0-9:]* '"${username}"' Exp \$"
'"${PLUS}"' new file'

		if $keep; then
		  echo Keeping ${TESTDIR} and exiting due to --keep
		  exit 0
		fi

		cd ..
		rm -r testimport
		rm -rf ${CVSROOT_DIRNAME}/trdiff
		;;

	rdiff-short)
	  # Test that the short patch behaves as expected
	  #   1) Added file.
	  #   2) Removed file.
	  #   3) Different revision number with no difference.
	  #   4) Different revision number with changes.
	  #   5) Against trunk.
	  #   6) Same revision number (no difference).
	  mkdir rdiff-short; cd rdiff-short
	  mkdir abc
	  dotest rdiff-short-init-1 \
"${testcvs} -q import -I ! -m initial-import abc vendor initial" \
'
No conflicts created by this import'

	  dotest rdiff-short-init-2 "${testcvs} -q get abc" ''
	  cd abc
	  echo "abc" >file1.txt
	  dotest rdiff-short-init-3 "${testcvs} add file1.txt" \
"${PROG} add: scheduling file .file1\.txt' for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rdiff-short-init-4 \
"${testcvs} commit -madd-file1 file1.txt" \
"RCS file: ${CVSROOT_DIRNAME}/abc/file1\.txt,v
done
Checking in file1\.txt;
${CVSROOT_DIRNAME}/abc/file1\.txt,v  <--  file1\.txt
initial revision: 1\.1
done"
	  echo def >>file1.txt
	  dotest rdiff-short-init-5 \
"${testcvs} commit -mchange-file1 file1.txt" \
"Checking in file1\.txt;
${CVSROOT_DIRNAME}/abc/file1\.txt,v  <--  file1\.txt
new revision: 1\.2; previous revision: 1\.1
done"
	  echo "abc" >file1.txt
	  dotest rdiff-short-init-6 \
"${testcvs} commit -mrestore-file1-rev1 file1.txt" \
"Checking in file1\.txt;
${CVSROOT_DIRNAME}/abc/file1\.txt,v  <--  file1\.txt
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest rdiff-short-init-7 \
"${testcvs} tag -r 1.1 tag1 file1.txt" \
"T file1\.txt"
	  dotest rdiff-short-init-8 \
"${testcvs} tag -r 1.2 tag2 file1.txt" \
"T file1\.txt"
	  dotest rdiff-short-init-9 \
"${testcvs} tag -r 1.3 tag3 file1.txt" \
"T file1\.txt"
	  echo "abc" >file2.txt
	  dotest rdiff-short-init-10 \
"${testcvs} add file2.txt" \
"${PROG} add: scheduling file .file2\.txt' for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rdiff-add-remove-nodiff-init-11 \
"${testcvs} commit -madd-file2 file2.txt" \
"RCS file: ${CVSROOT_DIRNAME}/abc/file2\.txt,v
done
Checking in file2\.txt;
${CVSROOT_DIRNAME}/abc/file2\.txt,v  <--  file2\.txt
initial revision: 1\.1
done"
	  dotest rdiff-short-init-12 \
"${testcvs} tag -r 1.1 tag4 file2.txt" \
"T file2\.txt"
	  dotest rdiff-short-init-13 \
"${testcvs} tag -r 1.1 tag5 file2.txt" \
"T file2\.txt"
	  cd ../..
	  rm -fr rdiff-short

	  # 3) Different revision number with no difference.
	  dotest rdiff-short-no-real-change \
"${testcvs} -q rdiff -s -r tag1 -r tag3 abc"

	  # 4) Different revision number with changes.
	  dotest rdiff-short-real-change \
"${testcvs} -q rdiff -s -r tag1 -r tag2 abc" \
'File abc/file1.txt changed from revision 1\.1 to 1\.2'

	  # 1) Added file.
	  # 2) Removed file.
	  dotest_sort rdiff-short-remove-add \
"${testcvs} -q rdiff -s -r tag2 -r tag4 abc" \
'File abc/file1\.txt is removed; tag2 revision 1\.2
File abc/file2\.txt is new; tag4 revision 1\.1'

	  # 6) Same revision number (no difference).
	  dotest rdiff-short-no-change \
"${testcvs} -q rdiff -s -r tag4 -r tag5 abc"

	  # 5) Against trunk.
	  # Check that the messages change when we diff against the trunk
	  # rather than a tag or date.
	  dotest rdiff-short-against-trunk-1 \
"${testcvs} -q rdiff -s -rtag4 abc" \
"File abc/file1\.txt is new; current revision 1\.3"

	  dotest rdiff-short-against-trunk-2 \
"${testcvs} -q rdiff -s -rtag2 abc" \
"File abc/file1\.txt changed from revision 1\.2 to 1\.3
File abc/file2\.txt is new; current revision 1\.1"

	  rm -rf ${CVSROOT_DIRNAME}/abc
	  ;;

	rdiff2)
	  # Test for the segv problem reported by James Cribb
	  # Somewhere to work
	  mkdir rdiff2; cd rdiff2	  
	  # Create a module "m" with files "foo" and "d/bar"
	  mkdir m; cd m
	  echo foo >foo
	  mkdir d
	  echo bar >d/bar
	  dotest_sort  rdiff2-1 \
"${testcvs} -q import -I ! -m initial-import m vendor initial" \
'

N m/d/bar
N m/foo
No conflicts created by this import'

	  cd ..
	  rm -r m
	  
	  # Remove "foo"
	  dotest rdiff2-2 "${testcvs} get m" \
"${PROG} checkout: Updating m
U m/foo
${PROG} checkout: Updating m/d
U m/d/bar"
	  cd m
	  dotest rdiff2-3 "${testcvs} rm -f foo" \
"${PROG} remove: scheduling .foo. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"

	  dotest rdiff2-4 "${testcvs} commit -m Removed foo" \
"Removing foo;
${CVSROOT_DIRNAME}/m/foo,v  <--  foo
new revision: delete; previous revision: 1\.1\.1\.1
done"
	  
	  # Modify "d/bar"
	  echo foo >d/bar
	  dotest rdiff2-5 "${testcvs} commit -m Changed d/bar" \
"Checking in d/bar;
${CVSROOT_DIRNAME}/m/d/bar,v  <--  bar
new revision: 1\.2; previous revision: 1\.1
done"
	  
	  # Crash before showing d/bar diffs
	  dotest_fail rdiff2-6 "${testcvs} rdiff -t m" \
"${PROG} rdiff: Diffing m
${PROG} rdiff: Diffing m/d
Index: m/d/bar
diff -c m/d/bar:1\.1\.1\.1 m/d/bar:1\.2
\*\*\* m/d/bar:1\.1\.1\.1	${DATE}
--- m/d/bar	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! bar
--- 1 ----
! foo"
	  if $keep; then
		echo Keeping ${TESTDIR} and exiting due to --keep
		exit 0
	  fi
	  cd ../..
	  rm -rf rdiff2
	  rm -rf ${CVSROOT_DIRNAME}/m
	  ;;

	diff)
	  # Various tests specific to the "cvs diff" command.
	  # Related tests:
	  #   death2: -N
	  #   rcslib: cvs diff and $Name.
	  #   rdiff: cvs rdiff.
	  #   diffmerge*: nuts and bolts (stuff within diff library)
	  mkdir 1; cd 1
	  dotest diff-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest diff-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  # diff is anomalous.  Most CVS commands print the "nothing
	  # known" message (or worse yet, no message in some cases) but
	  # diff says "I know nothing".  Shrug.
	  dotest_fail diff-3 "${testcvs} diff xyzpdq" \
"${PROG} diff: I know nothing about xyzpdq"
	  touch abc
	  dotest diff-4 "${testcvs} add abc" \
"${PROG} add: scheduling file .abc. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest diff-5 "${testcvs} -q ci -mtest" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
done
Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
initial revision: 1\.1
done"
	  echo "extern int gethostname ();" >abc
	  dotest diff-6 "${testcvs} -q ci -mtest" \
"Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
new revision: 1\.2; previous revision: 1\.1
done"
	  echo "#include <winsock.h>" >abc
	  # check the behavior of the --ifdef=MACRO option
	  dotest_fail diff-7 "${testcvs} -q diff --ifdef=HAVE_WINSOCK_H" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.2
diff --ifdef HAVE_WINSOCK_H -r1\.2 abc
#ifndef HAVE_WINSOCK_H
extern int gethostname ();
#else /\* HAVE_WINSOCK_H \*/
#include <winsock\.h>
#endif /\* HAVE_WINSOCK_H \*/"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 1
	  ;;

	diffnl)
	  # Test handling of 'cvs diff' of files without newlines
	  mkdir 1; cd 1
	  dotest diffnl-000 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest diffnl-001 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir

	  ${AWK} 'BEGIN {printf("one\ntwo\nthree\nfour\nfive\nsix")}' </dev/null >abc
	  dotest diffnl-002 "${testcvs} add abc" \
"${PROG} add: scheduling file .abc. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
          dotest diffnl-003 "${testcvs} -q ci -mtest" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
done
Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
initial revision: 1\.1
done"

	  # change to line near EOF
	  ${AWK} 'BEGIN {printf("one\ntwo\nthree\nfour\nsix")}' </dev/null >abc
	  dotest_fail diffnl-100 "${testcvs} diff abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.1
diff -r1\.1 abc
5d4
< five"
          dotest_fail diffnl-101 "${testcvs} diff -u abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.1
diff -u -r1\.1 abc
--- abc	${RFCDATE}	1\.1
+++ abc	${RFCDATE}
@@ -2,5 +2,4 @@
 two
 three
 four
-five
 six
\\\\ No newline at end of file"
          dotest diffnl-102 "${testcvs} -q ci -mtest abc" \
"Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
new revision: 1\.2; previous revision: 1\.1
done"

          # Change to last line
	  ${AWK} 'BEGIN {printf("one\ntwo\nthree\nfour\nseven")}' </dev/null >abc
          dotest_fail diffnl-200 "${testcvs} diff abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.2
diff -r1\.2 abc
5c5
< six
\\\\ No newline at end of file
---
> seven
\\\\ No newline at end of file"
	  dotest_fail diffnl-201 "${testcvs} diff -u abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.2
diff -u -r1\.2 abc
--- abc	${RFCDATE}	1\.2
+++ abc	${RFCDATE}
@@ -2,4 +2,4 @@
 two
 three
 four
-six
\\\\ No newline at end of file
+seven
\\\\ No newline at end of file"
	  dotest diffnl-202 "${testcvs} ci -mtest abc" \
"Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
new revision: 1\.3; previous revision: 1\.2
done"

	  # Addition of newline
	  echo "one
two
three
four
seven" > abc
	  dotest_fail diffnl-300 "${testcvs} diff abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.3
diff -r1\.3 abc
5c5
< seven
\\\\ No newline at end of file
---
> seven"
	  dotest_fail diffnl-301 "${testcvs} diff -u abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.3
diff -u -r1\.3 abc
--- abc	${RFCDATE}	1\.3
+++ abc	${RFCDATE}
@@ -2,4 +2,4 @@
 two
 three
 four
-seven
\\\\ No newline at end of file
+seven"
	  dotest diffnl-302 "${testcvs} ci -mtest abc" \
"Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
new revision: 1\.4; previous revision: 1\.3
done"

	  # Removal of newline
	  ${AWK} 'BEGIN {printf("one\ntwo\nthree\nfour\nseven")}' </dev/null >abc
	  dotest_fail diffnl-400 "${testcvs} diff abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.4
diff -r1\.4 abc
5c5
< seven
---
> seven
\\\\ No newline at end of file"
	  dotest_fail diffnl-401 "${testcvs} diff -u abc" \
"Index: abc
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
retrieving revision 1\.4
diff -u -r1\.4 abc
--- abc	${RFCDATE}	1\.4
+++ abc	${RFCDATE}
@@ -2,4 +2,4 @@
 two
 three
 four
-seven
+seven
\\\\ No newline at end of file"
	
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir added to the repository"
		cd subdir
		echo file in subdir >sfile
		dotest 65a1 "${testcvs} add sfile" \
"${PROG}"' add: scheduling file `sfile'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
		dotest 65a2 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/sfile,v
done
Checking in sfile;
${CVSROOT_DIRNAME}/first-dir/subdir/sfile,v  <--  sfile
initial revision: 1\.1
done"
		rm sfile
		dotest 65a3 "${testcvs} rm sfile" \
"${PROG}"' remove: scheduling `sfile'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove this file permanently'
		dotest 65a4 "${testcvs} -q ci -m remove-it" \
"Removing sfile;
${CVSROOT_DIRNAME}/first-dir/subdir/sfile,v  <--  sfile
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
"${PROG}"' add: scheduling file `file4'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
		dotest death-file4-ciadd "${testcvs} -q ci -m add file4" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
initial revision: 1\.1
done"
		rm file4
		dotest death-file4-rm "${testcvs} remove file4" \
"${PROG}"' remove: scheduling `file4'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove this file permanently'
		dotest death-file4-cirm "${testcvs} -q ci -m remove file4" \
"Removing file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
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
\*\*\* /dev/null	${DATE}
--- first-dir/file3	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} line1 from branch1"
		dotest death-76a1 \
"${testcvs} -q rdiff -r branch1 -r bp_branch1 first-dir" \
"Index: first-dir/file3
diff -c first-dir/file3:1\.1\.2\.1 first-dir/file3:removed
\*\*\* first-dir/file3:1\.1\.2\.1	${DATE}
--- first-dir/file3	${DATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- line1 from branch1
--- 0 ----"

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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.3\.2\.1
Merging differences between 1\.3 and 1\.3\.2\.1 into file1
${PROG} update: scheduling file2 for removal
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done
Removing file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
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
		  "${PROG}"' \[update aborted\]: no such tag brnach1'
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
		dotest 95 "$testcvs -q update -j HEAD" \
"$PROG update: file file1 has been removed in revision HEAD, but the destination is incompatibly modified
C file1
$PROG update: file file3 exists, but has been added in revision HEAD"

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

	  # Add two files on the trunk.
	  echo "first revision" > file1
	  echo "file4 first revision" > file4
	  dotest death2-2 "${testcvs} add file1 file4" \
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: scheduling file `file4'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'

	  dotest death2-3 "${testcvs} -q commit -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
initial revision: 1\.1
done"

	  # Make a branch and a non-branch tag.
	  dotest death2-4 "${testcvs} -q tag -b branch" \
'T file1
T file4'
	  dotest death2-5 "${testcvs} -q tag tag" \
'T file1
T file4'

	  # Switch over to the branch.
	  dotest death2-6 "$testcvs -q update -r branch" \
'[UP] file1
[UP] file4'

	  # Delete the file on the branch.
	  rm file1
	  dotest death2-7 "${testcvs} rm file1" \
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"

	  # Test diff of the removed file before it is committed.
	  dotest_fail death2-diff-1 "${testcvs} -q diff file1" \
"${PROG} diff: file1 was removed, no comparison available"

	  dotest_fail death2-diff-2 "${testcvs} -q diff -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* file1	${RFCDATE}	[0-9.]*
--- /dev/null	${RFCDATE_EPOCH}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  dotest death2-8 "${testcvs} -q ci -m removed" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"

	  # Test diff of a dead file.
	  dotest_fail death2-diff-3 \
"${testcvs} -q diff -r1.1 -rbranch -c file1" \
"${PROG} diff: Tag branch refers to a dead (removed) revision in file .file1.\.
${PROG} diff: No comparison available\.  Pass .-N. to .${PROG} diff.${QUESTION}"
	  # and in reverse
	  dotest_fail death2-diff-3a \
"${testcvs} -q diff -rbranch -r1.1 -c file1" \
"${PROG} diff: Tag branch refers to a dead (removed) revision in file .file1.\.
${PROG} diff: No comparison available\.  Pass .-N. to .${PROG} diff.${QUESTION}"

	  dotest_fail death2-diff-4 \
"${testcvs} -q diff -r1.1 -rbranch -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* file1	${RFCDATE}	[0-9.]*
--- /dev/null	${RFCDATE_EPOCH}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"
	  # and in reverse
	  dotest_fail death2-diff-4a \
"${testcvs} -q diff -rbranch -r1.1 -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* /dev/null	${RFCDATE_EPOCH}
--- file1	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
+ first revision"


	  dotest_fail death2-diff-5 "${testcvs} -q diff -rtag -c ." \
"${PROG} diff: file1 no longer exists, no comparison available"

	  dotest_fail death2-diff-6 "${testcvs} -q diff -rtag -N -c ." \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* file1	[-a-zA-Z0-9: ]*	[0-9.]*
--- /dev/null	${RFCDATE_EPOCH}
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
\*\*\* first-dir/file1:1\.1	[a-zA-Z0-9: ]*
--- first-dir/file1	[a-zA-Z0-9: ]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- first revision
--- 0 ----"

	  # Readd the file to the branch.
	  echo "second revision" > file1
	  dotest death2-9 "${testcvs} add file1" \
"${PROG}"' add: file `file1'\'' will be added on branch `branch'\'' from version 1\.1\.2\.1
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  # Test diff of the added file before it is committed.
	  dotest_fail death2-diff-7 "${testcvs} -q diff file1" \
"${PROG} diff: file1 is a new entry, no comparison available"

	  dotest_fail death2-diff-8 "${testcvs} -q diff -N -c file1" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* /dev/null	${RFCDATE_EPOCH}
--- file1	${RFCDATE}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} second revision"

	  dotest death2-10 "${testcvs} -q commit -m add" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # Delete file4 from the branch
	  dotest death2-10a "${testcvs} rm -f file4" \
"${PROG} remove: scheduling .file4. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest death2-10b "${testcvs} -q ci -m removed" \
"Removing file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1
done"

	  # Back to the trunk.
	  dotest death2-11 "${testcvs} -q update -A" \
"[UP] file1
U file4"

	  # Add another file on the trunk.
	  echo "first revision" > file2
	  dotest death2-12 "${testcvs} add file2" \
"${PROG}"' add: scheduling file `file2'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest death2-13 "${testcvs} -q commit -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  # Modify file4 on the trunk.
	  echo "new file4 revision" > file4
	  dotest death2-13a "${testcvs} -q commit -m mod" \
"Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done"

	  # Back to the branch.
	  # The ``no longer in the repository'' message doesn't really
	  # look right to me, but that's what CVS currently prints for
	  # this case.
	  dotest death2-14 "${testcvs} -q update -r branch" \
"[UP] file1
${PROG} update: file2 is no longer in the repository
${PROG} update: file4 is no longer in the repository"

	  # Add a file on the branch with the same name.
	  echo "branch revision" > file2
	  dotest death2-15 "${testcvs} add file2" \
"${PROG}"' add: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest death2-16 "${testcvs} -q commit -m add" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # Add a new file on the branch.
	  echo "first revision" > file3
	  dotest death2-17 "${testcvs} add file3" \
"${PROG}"' add: scheduling file `file3'\'' for addition on branch `branch'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest death2-18 "${testcvs} -q commit -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Test diff of a nonexistent tag
	  dotest_fail death2-diff-9 "${testcvs} -q diff -rtag -c file3" \
"${PROG} diff: tag tag is not in file file3"

	  dotest_fail death2-diff-10 "${testcvs} -q diff -rtag -N -c file3" \
"Index: file3
===================================================================
RCS file: file3
diff -N file3
\*\*\* /dev/null	${RFCDATE_EPOCH}
--- file3	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  dotest_fail death2-diff-11 "${testcvs} -q diff -rtag -c ." \
"Index: file1
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
diff -c -r1\.1 -r1\.1\.2\.2
\*\*\* file1	${RFCDATE}	[0-9.]*
--- file1	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! first revision
--- 1 ----
! second revision
${PROG} diff: tag tag is not in file file2
${PROG} diff: tag tag is not in file file3
${PROG} diff: file4 no longer exists, no comparison available"

	  dotest_fail death2-diff-12 "${testcvs} -q diff -rtag -c -N ." \
"Index: file1
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
diff -c -r1\.1 -r1\.1\.2\.2
\*\*\* file1	${RFCDATE}	[0-9.]*
--- file1	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! first revision
--- 1 ----
! second revision
Index: file2
===================================================================
RCS file: file2
diff -N file2
\*\*\* /dev/null	${RFCDATE_EPOCH}
--- file2	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} branch revision
Index: file3
===================================================================
RCS file: file3
diff -N file3
\*\*\* /dev/null	${RFCDATE_EPOCH}
--- file3	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision
Index: file4
===================================================================
RCS file: file4
diff -N file4
\*\*\* file4	${RFCDATE}	[0-9.]*
--- /dev/null	${RFCDATE_EPOCH}
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
- file4 first revision
--- 0 ----"

	  # Switch to the nonbranch tag.
	  dotest death2-19 "${testcvs} -q update -r tag" \
"[UP] file1
${PROG} update: file2 is no longer in the repository
${PROG} update: file3 is no longer in the repository
U file4"

	  dotest_fail death2-20 "test -f file2"

	  # Make sure diff only reports appropriate files.
	  dotest_fail death2-diff-13 "${testcvs} -q diff -r rdiff-tag" \
"${PROG} diff: Tag rdiff-tag refers to a dead (removed) revision in file .file1.\.
${PROG} diff: No comparison available\.  Pass .-N. to .${PROG} diff.${QUESTION}"

	  dotest_fail death2-diff-14 "${testcvs} -q diff -r rdiff-tag -c -N" \
"Index: file1
===================================================================
RCS file: file1
diff -N file1
\*\*\* /dev/null	${RFCDATE_EPOCH}
--- file1	${RFCDATE}	[0-9.]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 0 \*\*\*\*
--- 1 ----
${PLUS} first revision"

	  # now back to the trunk
	  dotest death2-21 "$testcvs -q update -A" \
'[UP] file1
U file2
U file4'

	  # test merging with a dead file
	  dotest death2-22 "${testcvs} -q co first-dir" \
"U first-dir/file1
U first-dir/file2
U first-dir/file4"

	  cd first-dir
	  dotest death2-23 "${testcvs} rm -f file4" \
"${PROG} remove: scheduling .file4. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest death2-24 "${testcvs} -q ci -m removed file4" \
"Removing file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.2
done"
	  cd ..
	  echo "new stuff" >file4
	  dotest_fail death2-25 "${testcvs} up file4" \
"${PROG} update: conflict: file4 is modified but no longer in the repository
C file4"

	  cd .. ; rm -rf first-dir ${CVSROOT_DIRNAME}/first-dir
	  ;;



	death-rtag)
	  # This documents a bug in CVS that prevents rtag from tagging files
	  # in the Attic.
	  mkdir $CVSROOT_DIRNAME/death-rtag
	  dotest death-rtag-init-1 "$testcvs -Q co death-rtag"
	  cd death-rtag
	  echo "This is the file foo" > foo
	  echo "This is the file bar" > bar
	  dotest death-rtag-init-2 "$testcvs -Q add foo bar"
	  dotest death-rtag-init-3 "$testcvs -Q ci -m 'Add foo and bar.'" \
"RCS file: $CVSROOT_DIRNAME/death-rtag/bar,v
done
Checking in bar;
$CVSROOT_DIRNAME/death-rtag/bar,v  <--  bar
initial revision: 1\.[0-9]*
done
RCS file: $CVSROOT_DIRNAME/death-rtag/foo,v
done
Checking in foo;
$CVSROOT_DIRNAME/death-rtag/foo,v  <--  foo
initial revision: 1\.[0-9]*
done"
	  dotest death-rtag-init-5 "$testcvs -Q tag -b mybranch"

	  dotest death-rtag-1 "$testcvs -q rtag -rmybranch willtag death-rtag"
	  dotest death-rtag-2 "$testcvs -Q rm -f foo"
	  dotest death-rtag-3 "$testcvs -Q ci -m 'Remove foo.'" \
"Removing foo;
$CVSROOT_DIRNAME/death-rtag/foo,v  <--  foo
new revision: delete; previous revision: 1\.[0-9]*
done"
	  # commit something on the branch so that the moving tag is visible.
	  dotest death-rtag-3.2 "$testcvs -Q up -rmybranch"
	  echo some branch content >>foo
	  echo some branch content >>bar
	  dotest death-rtag-3.3 "$testcvs -Q ci -m 'Change foo.'" \
"Checking in bar;
$CVSROOT_DIRNAME/death-rtag/bar,v  <--  bar
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in foo;
$CVSROOT_DIRNAME/death-rtag/Attic/foo,v  <--  foo
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest death-rtag-3.4 \
"$testcvs -q rtag -rmybranch wontmove death-rtag"
	  dotest death-rtag-3.5 "$testcvs -q rtag -F wontmove death-rtag"

	  cd ..
	  # Removing -f below avoids this bug.
	  dotest death-rtag-4 "$testcvs -q rtag -frmybranch wonttag death-rtag"

	  # When the bug existed, `wonttag' would not have been present in
	  # foo,v.
	  #
	  # A second bug prevented `wontmove' from moving from the branch to
	  # the dead revision on the trunk (death-rtag-3.4 & death-rtag-3.5).
	  dotest death-rtag-5 "$testcvs -q rlog death-rtag" \
"
RCS file: $CVSROOT_DIRNAME/death-rtag/bar,v
head: 1.[0-9]*
branch:
locks: strict
access list:
symbolic names:
	wonttag: 1\.1\.2\.1
	wontmove: 1\.1
	willtag: 1\.1
	mybranch: 1\.1.0\.2
keyword substitution: kv
$DOTSTAR
RCS file: $CVSROOT_DIRNAME/death-rtag/Attic/foo,v
head: 1.[0-9]*
branch:
locks: strict
access list:
symbolic names:
	wonttag: 1\.1\.2\.1
	wontmove: 1\.2
	willtag: 1\.1
	mybranch: 1\.1.0\.2
keyword substitution: kv
$DOTSTAR"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  rm -r death-rtag
	  rm -rf $CVSROOT_DIRNAME/death-rtag
	  ;;



	rm-update-message)
	  # FIXME
	  # local CVS prints a warning message when update notices a missing
	  # file and client/server CVS doesn't.  These should be identical.
	  mkdir rm-update-message; cd rm-update-message
	  mkdir $CVSROOT_DIRNAME/rm-update-message
	  dotest rm-update-message-setup-1 "$testcvs -q co rm-update-message" ''
	  cd rm-update-message
	  file=x
	  echo >$file
	  dotest rm-update-message-setup-2 "$testcvs -q add $file" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rm-update-message-setup-3 "$testcvs -q ci -mcreate $file" \
"RCS file: $CVSROOT_DIRNAME/rm-update-message/$file,v
done
Checking in $file;
$CVSROOT_DIRNAME/rm-update-message/$file,v  <--  $file
initial revision: 1\.1
done"

	  rm $file
	  dotest rm-update-message-1 "$testcvs up $file" \
"${PROG} update: warning: $file was lost
U $file"

	  cd ../..
	  if $keep; then :; else
	    rm -rf rm-update-message
	    rm -rf $CVSROOT_DIRNAME/rm-update-message
	  fi
	  ;;

	rmadd)
	  # More tests of adding and removing files.
	  # In particular ci -r.
	  # Other ci -r tests:
	  #   * editor-9: checking in a modified file,
	  #     where "ci -r" means a branch.
	  #   * basica-8a1: checking in a modified file with numeric revision.
	  #   * basica-8a2: likewise.
	  #   * keywordlog-4: adding a new file with numeric revision.
	  mkdir 1; cd 1
	  dotest rmadd-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest rmadd-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo first file1 >file1
	  dotest rmadd-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"

	  dotest_fail rmadd-4 "${testcvs} -q ci -r 1.2.2.4 -m add" \
"${PROG} commit: cannot add file .file1' with revision .1\.2\.2\.4'; must be on trunk
${PROG} \[commit aborted\]: correct above errors first!"
	  dotest_fail rmadd-5 "${testcvs} -q ci -r 1.2.2 -m add" \
"${PROG} commit: cannot add file .file1' with revision .1\.2\.2'; must be on trunk
${PROG} \[commit aborted\]: correct above errors first!"
	  dotest_fail rmadd-6 "${testcvs} -q ci -r mybranch -m add" \
"${PROG} \[commit aborted\]: no such tag mybranch"

	  # The thing with the trailing periods strikes me as a very
	  # bizarre behavior, but it would seem to be intentional
	  # (see commit.c).  It probably could go away....
	  dotest rmadd-7 "${testcvs} -q ci -r 7.... -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 7\.1
done"
	  if $remote; then
	    # I guess remote doesn't set a sticky tag in this case.
	    # Kind of odd, in the sense that rmadd-24a does set one
	    # both local and remote.
	    dotest_fail rmadd-7a "test -f CVS/Tag"
	    echo T7 >CVS/Tag
	  else
	    dotest rmadd-7a "cat CVS/Tag" "T7"
	  fi

	  dotest rmadd-8 "${testcvs} -q tag -b mybranch" "T file1"
	  dotest rmadd-9 "${testcvs} -q tag mynonbranch" "T file1"

	  touch file2
	  # The previous "cvs ci -r" set a sticky tag of '7'.  Seems a
	  # bit odd, and I guess commit.c (findmaxrev) makes '7' sticky
	  # tags unnecessary (?).  I kind of suspect that it should be
	  # saying "sticky tag is not a branch" like keywordlog-4b.
	  # Or something.
	  dotest rmadd-10 "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition on branch .7'
${PROG} add: use .${PROG} commit. to add this file permanently"
	  # As in the previous example, CVS is confused....
	  dotest rmadd-11 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 7\.1
done"

	  dotest rmadd-12 "${testcvs} -q update -A" ""
	  touch file3
	  dotest rmadd-13 "${testcvs} add file3" \
"${PROG} add: scheduling file .file3. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  # Huh?  file2 is not up to date?  Seems buggy to me....
	  dotest_fail rmadd-14 "${testcvs} -q ci -r mybranch -m add" \
"${PROG} commit: Up-to-date check failed for .file2'
${PROG} \[commit aborted\]: correct above errors first!"
	  # Whatever, let's not let file2 distract us....
	  dotest rmadd-15 "${testcvs} -q ci -r mybranch -m add file3" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  touch file4
	  dotest rmadd-16 "${testcvs} add file4" \
"${PROG} add: scheduling file .file4. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  # Same "Up-to-date check" issues as in rmadd-14.
	  # The "no such tag" thing is due to the fact that we only
	  # update val-tags when the tag is used (might be more of a
	  # bug than a feature, I dunno).
	  dotest_fail rmadd-17 \
"${testcvs} -q ci -r mynonbranch -m add file4" \
"${PROG} \[commit aborted\]: no such tag mynonbranch"
	  # Try to make CVS write val-tags.
	  dotest rmadd-18 "${testcvs} -q update -p -r mynonbranch file1" \
"first file1"
	  # Oops, -p suppresses writing val-tags (probably a questionable
	  # behavior).
	  dotest_fail rmadd-19 \
"${testcvs} -q ci -r mynonbranch -m add file4" \
"${PROG} \[commit aborted\]: no such tag mynonbranch"
	  # Now make CVS write val-tags for real.
	  dotest rmadd-20 "$testcvs -q update -r mynonbranch file1" '[UP] file1'
	  # Oops - CVS isn't distinguishing between a branch tag and
	  # a non-branch tag.
	  dotest rmadd-21 \
"${testcvs} -q ci -r mynonbranch -m add file4" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/Attic/file4,v  <--  file4
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # OK, we add this one in a vanilla way, but then check in
	  # a modification with ci -r and sniff around for sticky tags.
	  echo file5 >file5
	  dotest rmadd-22 "${testcvs} add file5" \
"${PROG} add: scheduling file .file5. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  if $remote; then
	    # Interesting bug (or missing feature) here.  findmaxrev
	    # gets the major revision from the Entries.  Well, remote
	    # doesn't send the entries for files which are not involved.
	    dotest rmadd-23r "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file5,v
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
initial revision: 1\.1
done"
	    dotest rmadd-23-workaroundr \
"${testcvs} -q ci -r 7 -m bump-it file5" \
"Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
new revision: 7\.1; previous revision: 1\.1
done"
	  else
	    dotest rmadd-23 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file5,v
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
initial revision: 7\.1
done"
	  fi
	  echo change it >file5
	  dotest_fail rmadd-24 "${testcvs} -q ci -r 4.8 -m change file5" \
"Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
${PROG} commit: ${CVSROOT_DIRNAME}/first-dir/file5,v: revision 4\.8 too low; must be higher than 7\.1
${PROG} commit: could not check in file5"
	  dotest rmadd-24a "${testcvs} -q ci -r 8.4 -m change file5" \
"Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
new revision: 8\.4; previous revision: 7\.1
done"
	  # I'm not really sure that a sticky tag make sense here.
	  # It seems to be longstanding behavior for what that is worth.
	  dotest rmadd-25 "${testcvs} status file5" \
"===================================================================
File: file5            	Status: Up-to-date

   Working revision:	8\.4.*
   Repository revision:	8\.4	${CVSROOT_DIRNAME}/first-dir/file5,v
   Sticky Tag:		8\.4
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  # now try forced revision with recursion
	  mkdir sub
	  dotest rmadd-26 "${testcvs} -q add sub" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sub added to the repository"
	  echo hello >sub/subfile
	  dotest rmadd-27 "${testcvs} -q add sub/subfile" \
"${PROG} add: use .${PROG} commit. to add this file permanently"

	  dotest rmadd-28 "${testcvs} -q ci -m. sub" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sub/subfile,v
done
Checking in sub/subfile;
${CVSROOT_DIRNAME}/first-dir/sub/subfile,v  <--  subfile
initial revision: 1\.1
done"

	  # lose the branch
	  dotest rmadd-29 "$testcvs -q up -A" \
"[UP] file1
$PROG update: file3 is no longer in the repository
$PROG update: file4 is no longer in the repository"

	  # -f disables recursion
	  dotest rmadd-30 "${testcvs} -q ci -f -r9 -m." \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 9\.1; previous revision: 7\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 9\.1; previous revision: 7\.1
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
new revision: 9\.1; previous revision: 8\.4
done"

	  # add -R to force recursion
	  dotest rmadd-31 "${testcvs} -q ci -f -r9 -R -m." \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 9\.2; previous revision: 9\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 9\.2; previous revision: 9\.1
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/file5,v  <--  file5
new revision: 9\.2; previous revision: 9\.1
done
Checking in sub/subfile;
${CVSROOT_DIRNAME}/first-dir/sub/subfile,v  <--  subfile
new revision: 9\.1; previous revision: 1\.1
done"

	  if $remote; then
	    # as noted above, remote doesn't set a sticky tag
	    :
	  else
	    dotest rmadd-32 "cat CVS/Tag" "T9"
	    dotest rmadd-33 "cat sub/CVS/Tag" "T9"
	  fi

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	rmadd2)
	  # Tests of undoing commits, including in the presence of
	  # adding and removing files.  See join for a list of -j tests.
	  mkdir 1; cd 1
	  dotest rmadd2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest rmadd2-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo 'initial contents' >file1
	  dotest rmadd2-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rmadd2-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest rmadd2-4a "${testcvs} -Q tag tagone" ""
	  dotest rmadd2-5 "${testcvs} rm -f file1" \
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest rmadd2-6 "${testcvs} -q ci -m remove" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  dotest rmadd2-7 "${testcvs} -q update -j 1.2 -j 1.1 file1" "U file1"
	  dotest rmadd2-8 "${testcvs} -q ci -m readd" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  echo 'new contents' >file1
	  dotest rmadd2-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"
	  dotest rmadd2-10 "${testcvs} -q update -j 1.4 -j 1.3 file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.4
retrieving revision 1\.3
Merging differences between 1\.4 and 1\.3 into file1"
	  dotest rmadd2-11 "${testcvs} -q ci -m undo" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest rmadd2-12 "cat file1" "initial contents"
	  dotest rmadd2-13 "${testcvs} -q update -p -r 1.3" "initial contents"

	  # Hmm, might be a bit odd that this works even if 1.3 is not
	  # the head.
	  dotest rmadd2-14 "${testcvs} -q update -j 1.3 -j 1.2 file1" \
"${PROG} update: scheduling file1 for removal"

	  # Check that -p can get arbitrary revisions of a removed file
	  dotest rmadd2-14a "${testcvs} -q update -p" "initial contents"
	  dotest rmadd2-14b "${testcvs} -q update -p -r 1.5" "initial contents"
	  dotest rmadd2-14c "${testcvs} -q update -p -r 1.3" "initial contents"

	  dotest rmadd2-15 "${testcvs} -q ci -m re-remove" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.5
done"
	  dotest rmadd2-16 "${testcvs} log -h file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
Working file: file1
head: 1\.6
branch:
locks: strict
access list:
symbolic names:
	tagone: 1\.1
keyword substitution: kv
total revisions: 6
============================================================================="
	  dotest rmadd2-17 "${testcvs} status -v file1" \
"===================================================================
File: no file file1		Status: Up-to-date

   Working revision:	No entry for file1
   Repository revision:	1\.6	${CVSROOT_DIRNAME}/first-dir/Attic/file1,v

   Existing Tags:
	tagone                   	(revision: 1.1)"

	  cd ../..

	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	rmadd3)
          # This test demonstrates that CVS notices that file1 exists rather
	  # that deleting or writing over it after:
	  #
	  #   cvs remove -f file1; touch file1; cvs add file1.
	  #
          # According to the manual, this should work for:
	  #
	  #   rm file1; cvs remove file1; cvs add file1
	  #
	  # but in past version of CVS, new content in file1 would be
	  # erroneously deleted when file1 reappeared between the remove and
	  # the add.
	  #
	  # Later versions of CVS would refuse to perform the add, but still
	  # allow a subsequent local commit to erase the file from the
	  # workspace, possibly losing data.
	  mkdir 1; cd 1
	  dotest rmadd3-init1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest rmadd3-init2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  echo initial content for file1 >file1
	  dotest rmadd3-init3 "${testcvs} add file1" \
"${PROG} add: scheduling file \`file1' for addition
${PROG} add: use '${PROG} commit' to add this file permanently"
	  dotest rmadd3-init4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  # Here begins the guts of this test, as detailed above.
	  dotest rmadd3-1 "${testcvs} rm -f file1" \
"${PROG} remove: scheduling \`file1' for removal
${PROG} remove: use '${PROG} commit' to remove this file permanently"

          # Now recreate the file:
	  echo desired future contents for file1 >file1

	  # And attempt to resurrect it at the same time:
	  dotest_fail rmadd3-2 "${testcvs} add file1" \
"${PROG} add: file1 should be removed and is still there (or is back again)"

	  # Now prove that commit knows that it shouldn't erase files.
	  dotest_fail rmadd3-3 "${testcvs} -q ci -m." \
"$PROG commit: \`file1' should be removed and is still there (or is back again)
$PROG \[commit aborted\]: correct above errors first!"

	  # Then these should pass too:
	  dotest rmadd3-4 "test -f file1"
	  dotest rmadd3-5 "cat file1" "desired future contents for file1"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	resurrection)
	  # This test tests a few file resurrection scenarios.
	  mkdir 1; cd 1
	  dotest resurrection-init1 "$testcvs -q co -l ." ''
	  mkdir first-dir
	  dotest resurrection-init2 "$testcvs add first-dir" \
"Directory $CVSROOT_DIRNAME/first-dir added to the repository"
	  cd first-dir

	  echo initial content for file1 >file1
	  dotest resurrection-init3 "$testcvs add file1" \
"$PROG add: scheduling file \`file1' for addition
$PROG add: use '$PROG commit' to add this file permanently"
	  dotest resurrection-init4 "$testcvs -q ci -m add" \
"RCS file: $CVSROOT_DIRNAME/first-dir/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  dotest resurrection-init5 "$testcvs -Q rm -f file1"

	  # The first test is that `cvs add' will resurrect a file before its
	  # removal has been committed.
	  dotest_sort resurrection-1 "$testcvs add file1" \
"U file1
$PROG add: file1, version 1\.1, resurrected"
	  dotest resurrection-2 "$testcvs -Q diff file1" ""

	  dotest resurrection-init6 "$testcvs -Q tag -b resurrection"
	  dotest resurrection-init7 "$testcvs -Q rm -f file1"
	  dotest resurrection-init8 "$testcvs -Q ci -mrm" \
"Removing file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"

	  # The next test is that CVS will resurrect a committed removal.
	  dotest_sort resurrection-3 "$testcvs add file1" \
"U file1
$PROG add: Re-adding file \`file1' (in place of dead revision 1\.2)\.
$PROG add: Resurrecting file \`file1' from revision 1\.1\.
$PROG add: use 'cvs commit' to add this file permanently"
	  dotest resurrection-4 "$testcvs -q diff -r1.1 file1" ""
	  dotest resurrection-5 "$testcvs -q ci -mreadd" \
"Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"

	  dotest resurrection-init9 "$testcvs -Q up -rresurrection"
	  dotest resurrection-init10 "$testcvs -Q rm -f file1"
	  dotest resurrection-init11 "$testcvs -Q ci -mrm-on-resurrection" \
"Removing file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"

	  # The next test is that CVS will resurrect a committed removal to a
	  # branch.
	  dotest_sort resurrection-6 "$testcvs add file1" \
"U file1
$PROG add: Resurrecting file \`file1' from revision 1\.1\.
$PROG add: file \`file1' will be added on branch \`resurrection' from version 1\.1\.2\.1
$PROG add: use 'cvs commit' to add this file permanently"
	  dotest resurrection-7 "$testcvs -Q diff -r1.1 file1" ""
	  dotest resurrection-8 "$testcvs -q ci -mreadd" \
"Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # The next few tests verify that an attempted resurrection of a file
	  # with no previous revision on the trunk fails.
	  touch file2
	  dotest resurrection-9 "$testcvs -Q add file2"
	  dotest resurrection-10 "$testcvs -Q ci -mnew-file2" \
"RCS file: $CVSROOT_DIRNAME/first-dir/Attic/file2,v
done
Checking in file2;
$CVSROOT_DIRNAME/first-dir/Attic/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest resurrection-11 "$testcvs -Q up -A"

	  # This command once caused an assertion failure.
	  dotest resurrection-12 "$testcvs add file2" \
"$PROG add: File \`file2' has no previous revision to resurrect\."

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -r 1
	  rm -rf $CVSROOT_DIRNAME/first-dir
	  ;;

	dirs)
	  # Tests related to removing and adding directories.
	  # See also:
	  #   conflicts (especially dir1 in conflicts-130): What happens if
	  #     directory exists in repository and a non-CVS-controlled
	  #     directory in the working directory?
	  #   conflicts3-15.  More cases, especially where CVS directory
	  #     exists but without CVS/Repository and friends.
	  #   conflicts3-22.  Similar to conflicts-130 but there is a file
	  #     in the directory.
	  #   dirs2.  Sort of similar to conflicts3-22 but somewhat different.
	  mkdir imp-dir; cd imp-dir
	  echo file1 >file1
	  mkdir sdir
	  echo sfile >sdir/sfile
	  dotest_sort dirs-1 \
"${testcvs} import -m import-it dir1 vend rel" "

N dir1/file1
N dir1/sdir/sfile
No conflicts created by this import
${PROG} import: Importing ${CVSROOT_DIRNAME}/dir1/sdir"
	  cd ..

	  mkdir 1; cd 1
	  dotest dirs-2 "${testcvs} -Q co dir1" ""

	  # Various CVS administrators are in the habit of removing
	  # the repository directory for things they don't want any
	  # more.  I've even been known to do it myself (on rare
	  # occasions).  Not the usual recommended practice, but we want
	  # to try to come up with some kind of reasonable/documented/sensible
	  # behavior.
	  rm -rf ${CVSROOT_DIRNAME}/dir1/sdir

	  dotest dirs-3 "${testcvs} update" \
"${PROG} update: Updating dir1
${PROG} update: Updating dir1/sdir
${PROG} update: cannot open directory ${CVSROOT_DIRNAME}/dir1/sdir: No such file or directory
${PROG} update: skipping directory dir1/sdir"
	  dotest dirs-3a "${testcvs} update -d" \
"${PROG} update*: Updating dir1
${PROG} update: Updating dir1/sdir
${PROG} update: cannot open directory ${CVSROOT_DIRNAME}/dir1/sdir: No such file or directory
${PROG} update: skipping directory dir1/sdir"

	  # If we say "yes", then CVS gives errors about not being able to
	  # create lock files.
	  # The fact that it says "skipping directory " rather than
	  # "skipping directory dir1/sdir" is some kind of bug.
	  dotest dirs-4 "echo no | ${testcvs} release -d dir1/sdir" \
"${PROG} update: cannot open directory ${CVSROOT_DIRNAME}/dir1/sdir: No such file or directory
${PROG} update: skipping directory 
You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .dir1/sdir': .. .release' aborted by user choice."

	  # OK, if "cvs release" won't help, we'll try it the other way...
	  rm -r dir1/sdir

	  dotest dirs-5 "cat dir1/CVS/Entries" \
"/file1/1.1.1.1/[a-zA-Z0-9 :]*//
D/sdir////"
	  dotest dirs-6 "${testcvs} update" "${PROG} update: Updating dir1"
	  dotest dirs-7 "cat dir1/CVS/Entries" \
"/file1/1.1.1.1/[a-zA-Z0-9 :]*//
D/sdir////"
	  dotest dirs-8 "${testcvs} update -d dir1" \
"${PROG} update: Updating dir1"

	  cd ..

	  rm -r imp-dir 1

	  # clean up our repositories
	  rm -rf ${CVSROOT_DIRNAME}/dir1
	  ;;

	dirs2)
	  # See "dirs" for a list of tests involving adding and
	  # removing directories.
	  mkdir 1; cd 1
	  dotest dirs2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest dirs2-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  mkdir sdir
	  dotest dirs2-3 "${testcvs} add sdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir added to the repository"
	  touch sdir/file1
	  dotest dirs2-4 "${testcvs} add sdir/file1" \
"${PROG} add: scheduling file .sdir/file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest dirs2-5 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/file1,v
done
Checking in sdir/file1;
${CVSROOT_DIRNAME}/first-dir/sdir/file1,v  <--  file1
initial revision: 1\.1
done"
	  rm -r sdir/CVS
	  if $remote; then
	    # This is just like conflicts3-23
	    dotest_fail dirs2-6 "${testcvs} update -d" \
"${QUESTION} sdir
${PROG} update: Updating \.
${PROG} update: Updating sdir
${PROG} update: move away sdir/file1; it is in the way
C sdir/file1"
	    rm sdir/file1
	    rm -r sdir/CVS

	    # This is where things are not just like conflicts3-23
	    dotest dirs2-7 "${testcvs} update -d" \
"${QUESTION} sdir
${PROG} update: Updating \.
${PROG} update: Updating sdir
U sdir/file1"
	  else
	    dotest dirs2-6 "${testcvs} update -d" \
"${PROG} update: Updating \.
${QUESTION} sdir"
	    rm sdir/file1
	    dotest dirs2-7 "${testcvs} update -d" \
"${PROG} update: Updating \.
${QUESTION} sdir"
	  fi
	  cd ../..

	  # Now, the same thing (more or less) on a branch.
	  mkdir 2; cd 2
	  dotest dirs2-8 "${testcvs} -q co first-dir" 'U first-dir/sdir/file1'
	  cd first-dir
	  dotest dirs2-9 "${testcvs} -q tag -b br" "T sdir/file1"
	  rm -r sdir/CVS
	  if $remote; then
	    # Cute little quirk of val-tags; if we don't recurse into
	    # the directories where the tag is defined, val-tags won't
	    # get updated.
	    dotest_fail dirs2-10 "${testcvs} update -d -r br" \
"${QUESTION} sdir
${PROG} \[update aborted\]: no such tag br"
	    dotest dirs2-10ar \
"${testcvs} -q rdiff -u -r 1.1 -r br first-dir/sdir/file1"
	    dotest_fail dirs2-10-again "${testcvs} update -d -r br" \
"${QUESTION} sdir
${PROG} update: Updating \.
${PROG} update: Updating sdir
${PROG} update: move away sdir/file1; it is in the way
C sdir/file1"
	  else
	    dotest_fail dirs2-10 "${testcvs} update -d -r br" \
"${PROG} update: in directory sdir:
${PROG} \[update aborted\]: there is no version here; do '${PROG} checkout' first"
	  fi
	  cd ../..

	  # OK, the above tests make the situation somewhat harder
	  # than it might be, in the sense that they actually have a
	  # file which is alive on the branch we are updating.  Let's
	  # try it where it is just a directory where all the files
	  # have been removed.
	  mkdir 3; cd 3
	  dotest dirs2-11 "${testcvs} -q co -r br first-dir" \
"U first-dir/sdir/file1"
	  cd first-dir
	  # Hmm, this doesn't mention the branch like add does.  That's
	  # an odd non-orthogonality.
	  dotest dirs2-12 "${testcvs} rm -f sdir/file1" \
"${PROG} remove: scheduling .sdir/file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest dirs2-13 "${testcvs} -q ci -m remove" \
"Removing sdir/file1;
${CVSROOT_DIRNAME}/first-dir/sdir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  cd ../../2/first-dir
	  if $remote; then
	    dotest dirs2-14 "${testcvs} update -d -r br" \
"${QUESTION} sdir/file1
${PROG} update: Updating \.
${PROG} update: Updating sdir"
	  else
	    dotest dirs2-14 "${testcvs} update -d -r br" \
"${PROG} update: Updating \.
${QUESTION} sdir"
	  fi
	  cd ../..

	  rm -r 1 2 3
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: scheduling file `file2'\'' for addition
'"${PROG}"' add: scheduling file `file3'\'' for addition
'"${PROG}"' add: scheduling file `file4'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'
	  dotest branches-2a "${testcvs} -n -q ci -m dont-commit" ""
	  dotest_lit branches-3 "${testcvs} -q ci -m add-it" <<HERE
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
initial revision: 1.1
done
HERE
	  echo 4:trunk-2 >file4
	  dotest branches-3.2 "${testcvs} -q ci -m trunk-before-branch" \
"Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done"
	  # The "cvs log file4" in test branches-14.3 will test that we
	  # didn't really add the tag.
	  dotest branches-3.3 "${testcvs} -qn tag dont-tag" \
"T file1
T file2
T file3
T file4"
	  # Modify this file before branching, to deal with the case where
	  # someone is hacking along, says "oops, I should be doing this on
	  # a branch", and only then creates the branch.
	  echo 1:br1 >file1
	  dotest branches-4 "${testcvs} tag -b br1" "${PROG}"' tag: Tagging \.
T file1
T file2
T file3
T file4'
	  dotest branches-5 "$testcvs update -r br1" \
"$PROG update: Updating \.
M file1
[UP] file2
[UP] file3
[UP] file4"
	  echo 2:br1 >file2
	  echo 4:br1 >file4
	  dotest branches-6 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"
	  dotest branches-7 "${testcvs} -q tag -b brbr" 'T file1
T file2
T file3
T file4'
	  dotest branches-8 "$testcvs -q update -r brbr" \
'[UP] file1
[UP] file2
[UP] file3
[UP] file4'
	  echo 1:brbr >file1
	  echo 4:brbr >file4
	  dotest branches-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1\.2\.1; previous revision: 1\.1\.2\.1
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.1\.2\.1; previous revision: 1\.2\.2\.1
done"
	  dotest branches-10 "cat file1 file2 file3 file4" '1:brbr
2:br1
3:ancest
4:brbr'
	  dotest branches-11 "$testcvs -q update -r br1" \
'U file1
[UP] file2
[UP] file3
U file4'
	  dotest branches-12 "cat file1 file2 file3 file4" '1:br1
2:br1
3:ancest
4:br1'
	  echo 4:br1-2 >file4
	  dotest branches-12.2 "${testcvs} -q ci -m change-on-br1" \
"Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done"
	  dotest branches-13 "${testcvs} -q update -A" \
'U file1
U file2
[UP] file3
U file4'
	  dotest branches-14 "cat file1 file2 file3 file4" '1:ancest
2:ancest
3:ancest
4:trunk-2'
	  echo 4:trunk-3 >file4
	  dotest branches-14.2 \
	    "${testcvs} -q ci -m trunk-change-after-branch" \
"Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest branches-14.3 "${testcvs} log file4" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
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
	  dotest_fail branches-14.4 \
	    "${testcvs} diff -c -r 1.1 -r 1.3 file4" \
"Index: file4
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.3
diff -c -r1\.1 -r1\.3
\*\*\* file4	${RFCDATE}	1\.1
--- file4	${RFCDATE}	1\.3
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:trunk-3"
	  dotest_fail branches-14.5 \
	    "${testcvs} diff -c -r 1.1 -r 1.2.2.1 file4" \
"Index: file4
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
retrieving revision 1\.1
retrieving revision 1\.2\.2\.1
diff -c -r1\.1 -r1\.2\.2\.1
\*\*\* file4	${RFCDATE}	1\.1
--- file4	${RFCDATE}	1\.2\.2\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1 \*\*\*\*
! 4:trunk-1
--- 1 ----
! 4:br1"
	  dotest branches-15 \
	    "${testcvs} update -j 1.1.2.1 -j 1.1.2.1.2.1 file1" \
	    "RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.1\.2\.1
retrieving revision 1\.1\.2\.1\.2\.1
Merging differences between 1\.1\.2\.1 and 1\.1\.2\.1\.2\.1 into file1
rcsmerge: warning: conflicts during merge"
	  dotest branches-16 "cat file1" '<<<<<<< file1
1:ancest
[=]======
1:brbr
[>]>>>>>> 1\.1\.2\.1\.2\.1'

	  dotest branches-o1 "${testcvs} -q admin -o ::brbr" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file3,v
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done"
	  cd ..

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r first-dir
	  ;;

	branches2)
	  # More branch tests.
	  # Test that when updating a new subdirectory in a directory
	  # which was checked out on a branch, the new subdirectory is
	  # created on the appropriate branch.  Test this when joining
	  # as well.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir trunk; cd trunk

	  # Create a file.
	  dotest branches2-1 "${testcvs} -q co first-dir"
	  cd first-dir
	  echo "file1 first revision" > file1
	  dotest branches2-2 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest branches2-3 "${testcvs} commit -m add file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  # Tag the file.
	  dotest branches2-4 "${testcvs} -q tag tag1" 'T file1'

	  # Make two branches.
	  dotest branches2-5 "${testcvs} -q rtag -b -r tag1 b1 first-dir" ''
	  dotest branches2-6 "${testcvs} -q rtag -b -r tag1 b2 first-dir" ''

	  # Create some files and a subdirectory on branch b1.
	  cd ../..
	  mkdir b1; cd b1
	  dotest branches2-7 "${testcvs} -q co -r b1 first-dir" \
"U first-dir/file1"
	  cd first-dir
	  echo "file2 first revision" > file2
	  dotest branches2-8 "${testcvs} add file2" \
"${PROG}"' add: scheduling file `file2'\'' for addition on branch `b1'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  mkdir dir1
	  dotest branches2-9 "${testcvs} add dir1" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir1 added to the repository
--> Using per-directory sticky tag "'`'"b1'"
	  echo "file3 first revision" > dir1/file3
	  dotest branches2-10 "${testcvs} add dir1/file3" \
"${PROG}"' add: scheduling file `dir1/file3'\'' for addition on branch `b1'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest branches2-11 "${testcvs} -q ci -madd ." \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/Attic/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir1/Attic/file3,v
done
Checking in dir1/file3;
${CVSROOT_DIRNAME}/first-dir/dir1/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Check out the second branch, and update the working
	  # directory to the first branch, to make sure the right
	  # happens with dir1.
	  cd ../..
	  mkdir b2; cd b2
	  dotest branches2-12 "${testcvs} -q co -r b2 first-dir" \
'U first-dir/file1'
	  cd first-dir
	  dotest branches2-13 "${testcvs} update -d -r b1 dir1" \
"${PROG} update: Updating dir1
U dir1/file3"
	  dotest branches2-14 "${testcvs} -q status" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		b2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1\.2\.1.*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/dir1/Attic/file3,v
   Sticky Tag:		b1 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  # FIXME: Just clobbering the directory like this is a bit
	  # tacky, although people generally expect it to work.  Maybe
	  # we should release it instead.  We do it a few other places
	  # below as well.
	  rm -r dir1
	  dotest branches2-15 "${testcvs} update -d -j b1 dir1" \
"${PROG} update: Updating dir1
U dir1/file3"
	  # FIXCVS: The `No revision control file' stuff seems to be
	  # CVS's way of telling us that we're adding the file on a
	  # branch, and the file is not on that branch yet.  This
	  # should be nicer.
	  dotest branches2-16 "${testcvs} -q status" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		b2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		b2 - MISSING from RCS file!
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  cd ../../trunk/first-dir
	  dotest branches2-17 "${testcvs} update -d -P dir1" \
"${PROG} update: Updating dir1"
	  dotest_fail branches2-18 "test -d dir1"
	  dotest branches2-19 "${testcvs} update -d -P -r b1 dir1" \
"${PROG} update: Updating dir1
U dir1/file3"
	  dotest branches2-20 "${testcvs} -q status" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1\.2\.1.*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/dir1/Attic/file3,v
   Sticky Tag:		b1 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  rm -r dir1
	  dotest branches2-21 "${testcvs} update -d -P -j b1 dir1" \
"${PROG} update: Updating dir1
U dir1/file3"
	  dotest branches2-22 "${testcvs} -q status" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/dir1/Attic/file3,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  cd ../..
	  rm -r b1 b2

	  # Check out branch b1 twice.  Crate a new directory in one
	  # working directory, then do a cvs update in the other
	  # working directory and see if the tags are right.
	  mkdir b1a
	  mkdir b1b
	  cd b1b
	  dotest branches2-23 "${testcvs} -q co -r b1 first-dir" \
'U first-dir/file1
U first-dir/file2
U first-dir/dir1/file3'
	  cd ../b1a
	  dotest branches2-24 "${testcvs} -q co -r b1 first-dir" \
'U first-dir/file1
U first-dir/file2
U first-dir/dir1/file3'
	  cd first-dir
	  mkdir dir2
	  dotest branches2-25 "${testcvs} add dir2" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir2 added to the repository
--> Using per-directory sticky tag "'`'"b1'"
	  echo "file4 first revision" > dir2/file4
	  dotest branches2-26 "${testcvs} add dir2/file4" \
"${PROG}"' add: scheduling file `dir2/file4'\'' for addition on branch `b1'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest branches2-27 "${testcvs} -q commit -madd" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/dir2/Attic/file4,v
done
Checking in dir2/file4;
${CVSROOT_DIRNAME}/first-dir/dir2/Attic/file4,v  <--  file4
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  cd ../../b1b/first-dir
	  dotest branches2-28 "${testcvs} update -d dir2" \
"${PROG} update: Updating dir2
U dir2/file4"
	  cd dir2
	  dotest branches2-29 "${testcvs} -q status" \
"===================================================================
File: file4            	Status: Up-to-date

   Working revision:	1\.1\.2\.1.*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/dir2/Attic/file4,v
   Sticky Tag:		b1 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest branches2-30 "cat CVS/Tag" 'Tb1'

	  # Test update -A on a subdirectory
	  cd ..
	  rm -r dir2
	  dotest branches2-31 "${testcvs} update -A -d dir2" \
"${PROG} update: Updating dir2"
	  cd dir2
	  dotest branches2-32 "${testcvs} -q status" ''
	  dotest_fail branches2-33 "test -f CVS/Tag"

	  # Add a file on the trunk.
	  echo "file5 first revision" > file5
	  dotest branches2-34 "${testcvs} add file5" \
"${PROG}"' add: scheduling file `file5'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest branches2-35 "${testcvs} -q commit -madd" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/dir2/file5,v
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/dir2/file5,v  <--  file5
initial revision: 1\.1
done"

	  cd ../../../trunk/first-dir
	  dotest branches2-36 "${testcvs} -q update -d dir2" 'U dir2/file5'
	  cd dir2
	  dotest branches2-37 "${testcvs} -q status" \
"===================================================================
File: file5            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/dir2/file5,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest_fail branches2-38 "test -f CVS/status"

	  cd ../../..
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r trunk b1a b1b
	  ;;

	tagc)
	  # Test the tag -c option.
	  mkdir 1; cd 1
	  dotest tagc-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest tagc-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch file1 file2
	  dotest tagc-3 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest tagc-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  dotest tagc-5 "${testcvs} -q tag -c tag1" \
"T file1
T file2"
	  touch file1 file2
	  dotest tagc-6 "${testcvs} -q tag -c tag2" \
"T file1
T file2"
	  # Avoid timestamp granularity bugs (FIXME: CVS should be
	  # doing the sleep, right?).
	  sleep 1
	  echo myedit >>file1
	  dotest tagc-6a "${testcvs} rm -f file2" \
"${PROG} remove: scheduling .file2. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  touch file3
	  dotest tagc-6b "${testcvs} add file3" \
"${PROG} add: scheduling file .file3. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest_fail tagc-7 "${testcvs} -q tag -c tag3" \
"${PROG} tag: file1 is locally modified
${PROG} tag: file2 is locally modified
${PROG} tag: file3 is locally modified
${PROG} \[tag aborted\]: correct the above errors first!"
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest tagc-8 "${testcvs} -q co first-dir" \
"U first-dir/file1
U first-dir/file2"
	  cd ../1/first-dir
	  dotest tagc-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
Removing file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
initial revision: 1\.1
done"
	  cd ../../2/first-dir
	  dotest tagc-10 "${testcvs} -q tag -c tag4" \
"${PROG} tag: file2 is no longer in the repository
T file1
T file2"
	  cd ../..

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	update-p)
	  # Make sure `cvs update -p -rT FILE' works from a branch when
	  # FILE is already on the trunk and is being added to that branch.

	  mkdir 1; cd 1
	  module=x

	  echo > unused-file

	  # Create the module.
	  dotest update-p-1 \
	    "$testcvs -Q import -m. $module X Y" ''

	  file=F
	  # Check it out and tag it.
	  dotest update-p-2 "$testcvs -Q co $module" ''
	  cd $module
	  dotest update-p-3 "$testcvs -Q tag -b B" ''
	  echo v1 > $file
	  dotest update-p-4 "$testcvs -Q add $file" ''
	  dotest update-p-5 "$testcvs -Q ci -m. $file" \
"RCS file: ${CVSROOT_DIRNAME}/$module/$file,v
done
Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
initial revision: 1\.1
done"
	  dotest update-p-6 "$testcvs -Q tag T $file" ''
	  dotest update-p-7 "$testcvs -Q update -rB" ''

	  # This merge effectively adds file F on branch B.
	  dotest update-p-8 "$testcvs -Q update -jT" ''

	  # Before the fix that prompted the addition of this test,
	  # the following command would fail with this diagnostic:
	  # cvs update: conflict: F created independently by second party
	  dotest update-p-9 "$testcvs update -p -rT $file" \
"===================================================================
Checking out $file
RCS:  ${CVSROOT_DIRNAME}/$module/$file,v
VERS: 1\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
v1"

	  # Repeat the above, but with $file removed.
	  # This exercises a slightly different code path.
	  rm $file
	  # Before the fix that prompted the addition of this test,
	  # the following command would fail with this diagnostic:
	  # cvs update: warning: new-born F has disappeared
	  dotest update-p-10 "$testcvs update -p -rT $file" \
"===================================================================
Checking out $file
RCS:  ${CVSROOT_DIRNAME}/$module/$file,v
VERS: 1\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
v1"

	  # Exercise yet another code path:
	  # the one that involves reviving a `dead' file.
	  # And a little more, for good measure...
	  touch new
	  dotest update-p-a1 "$testcvs -Q add new" ''
	  dotest update-p-a2 "$testcvs -Q update -p new" ''
	  dotest update-p-a3 "$testcvs -Q rm -f new" ''

	  # Both an update -A, *and* the following update are required
	  # to return to the state of being on the trunk with a $file
	  # that we can then remove.
	  dotest update-p-undead-0 "$testcvs update -A" \
"$PROG update: Updating \.
$PROG update: warning: new-born $file has disappeared
[UP] unused-file"
	  dotest update-p-undead-1 "$testcvs update" \
"${PROG} update: Updating \.
U $file"
	  dotest update-p-undead-2 "$testcvs -Q update -p -rT $file" v1
	  dotest update-p-undead-3 "$testcvs -Q rm -f $file" ''
	  dotest update-p-undead-4 "$testcvs -Q update -p -rT $file" v1
	  dotest update-p-undead-5 "$testcvs -Q ci -m. $file" \
"Removing $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
new revision: delete; previous revision: 1\.1
done"
	  dotest update-p-undead-6 "$testcvs -Q update -p -rT $file" v1
	  echo v2 > $file
	  dotest update-p-undead-7 "$testcvs -Q update -p -rT $file" v1
	  dotest update-p-undead-8 "$testcvs add $file" \
"${PROG} add: Re-adding file .$file. (in place of dead revision 1\.2)\.
${PROG} add: use .${PROG} commit. to add this file permanently"

	  dotest update-p-undead-9 "$testcvs -Q update -p -rT $file" v1

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	tagf)
	  # More tagging tests, including using tag -F -B to convert a
	  # branch tag to a regular tag and recovering thereof.

	  # Setup; check in first-dir/file1
	  mkdir 1; cd 1
	  dotest tagf-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest tagf-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch file1 file2
	  dotest tagf-3 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest tagf-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  # Now create a branch and commit a revision there.
	  dotest tagf-5 "${testcvs} -q tag -b br" "T file1
T file2"
	  dotest tagf-6 "$testcvs -q update -r br" \
'U file1
U file2'
	  echo brmod >> file1
	  echo brmod >> file2
	  dotest tagf-7 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  # Here we try to make it a non-branch tag, but will
	  # succeed in getting only warnings, even with -F 
	  # because converting a branch tag to non-branch 
	  # is potentially catastrophic.
	  dotest tagf-8a "${testcvs} -q tag -F br" \
"${PROG} tag: file1: Not moving branch tag .br. from 1\.1\.2\.1 to 1\.1\\.2\.1\.
${PROG} tag: file2: Not moving branch tag .br. from 1\.1\.2\.1 to 1\.1\.2\.1\."
	  # however, if we *really* are sure we want to move a branch tag,
	  # "-F -B" will do the trick
	  dotest tagf-8 "${testcvs} -q tag -F -B br" "T file1
T file2"
	  echo moremod >> file1
	  echo moremod >> file2
	  dotest tagf-9 "${testcvs} -q status -v file1" \
"===================================================================
File: file1            	Status: Locally Modified

   Working revision:	1\.1\.2\.1.*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br (revision: 1\.1\.2\.1)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	br                       	(revision: 1\.1\.2\.1)"

	  # Now, how do we recover?
	  dotest tagf-10 "${testcvs} -q tag -d br" "D file1
D file2"
	  # This creates a new branch, 1.1.4.  See the code in RCS_magicrev
	  # which will notice that there is a (non-magic) 1.1.2 and thus
	  # skip that number.
	  dotest tagf-11 "${testcvs} -q tag -r 1.1 -b br file1" "T file1"
	  # Fix it with admin -n (cf admin-18, admin-26-4).
	  dotest tagf-12 "${testcvs} -q admin -nbr:1.1.2 file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
	  # Another variation on the file2 test would be to use two working
	  # directories so that the update -r br would need to
	  # a merge to get from 1.1.2.1 to the head of the 1.1.2 branch.
	  dotest tagf-13 "${testcvs} -q update -r br" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.1\.2\.1
retrieving revision 1\.1
Merging differences between 1\.1\.2\.1 and 1\.1 into file1
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in file1
C file1
M file2"
	  # CVS is giving a conflict because we are trying to get back to
	  # 1.1.4.  I'm not sure why it is a conflict rather than just
	  # "M file1".
	  dotest tagf-14 "cat file1" \
"<<<<<<< file1
brmod
moremod
[=]======
[>]>>>>>> 1\.1"
	  echo resolve >file1
	  dotest tagf-15 "${testcvs} -q ci -m recovered" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"
	  # try accidentally deleting branch tag, "tag -d"
	  dotest_fail tagf-16 "${testcvs} tag -d br" \
"${PROG} tag: Untagging \.
${PROG} tag: Not removing branch tag .br. from .${CVSROOT_DIRNAME}/first-dir/file1,v.\.
${PROG} tag: Not removing branch tag .br. from .${CVSROOT_DIRNAME}/first-dir/file2,v.\."
	  # try accidentally deleting branch tag, "rtag -d"
	  dotest_fail tagf-17 "${testcvs} rtag -d br first-dir" \
"${PROG} rtag: Untagging first-dir
${PROG} rtag: Not removing branch tag .br. from .${CVSROOT_DIRNAME}/first-dir/file1,v.\.
${PROG} rtag: Not removing branch tag .br. from .${CVSROOT_DIRNAME}/first-dir/file2,v.\."
	  # try accidentally converting branch tag to non-branch tag "tag -F"
	  dotest tagf-18 "${testcvs} tag -r1.1 -F br file1" \
"${PROG} tag: file1: Not moving branch tag .br. from 1\.1\.4\.1 to 1\.1\."
	  # try accidentally converting branch tag to non-branch tag "rtag -F"
	  dotest tagf-19 "${testcvs} rtag -r1.1 -F br first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: first-dir/file1: Not moving branch tag .br. from 1\.1\.4\.1 to 1\.1\.
${PROG} rtag: first-dir/file2: Not moving branch tag .br. from 1\.1\.2\.2 to 1\.1\."
	  # create a non-branch tag
	  dotest tagf-20 "${testcvs} rtag regulartag first-dir" \
"${PROG} rtag: Tagging first-dir"
	  # try accidentally converting non-branch tag to branch tag (tag -F -B -b)
	  dotest tagf-21 "${testcvs} tag -F -B -b regulartag file1" \
"${PROG} tag: file1: Not moving non-branch tag .regulartag. from 1\.1 to 1\.1\.4\.1\.0\.2 due to .-B. option\."
	  # try accidentally converting non-branch tag to branch rtag (rtag -F -B -b)
	  dotest tagf-22 "${testcvs} rtag -F -B -b regulartag first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: first-dir/file1: Not moving non-branch tag .regulartag. from 1\.1 to 1\.1\.0\.6 due to .-B. option\.
${PROG} rtag: first-dir/file2: Not moving non-branch tag .regulartag. from 1\.1 to 1\.1\.0\.4 due to .-B. option\."
	  # Try accidentally deleting non-branch: (tag -d -B)
	  dotest_fail tagf-23 "${testcvs} tag -d -B regulartag file1" \
"${PROG} tag: Not removing non-branch tag .regulartag. from .${CVSROOT_DIRNAME}/first-dir/file1,v. due to .-B. option\."
	  # Try accidentally deleting non-branch: (rtag -d -B)
	  dotest_fail tagf-24 \
		"${testcvs} rtag -d -B regulartag first-dir" \
"${PROG} rtag: Untagging first-dir
${PROG} rtag: Not removing non-branch tag .regulartag. from .${CVSROOT_DIRNAME}/first-dir/file1,v. due to .-B. option\.
${PROG} rtag: Not removing non-branch tag .regulartag. from .${CVSROOT_DIRNAME}/first-dir/file2,v. due to .-B. option\."

	  # the following tests (throught the next commit) keep moving the same
	  # tag back and forth between 1.1.6 & 1.1.8  in file1 and between
	  # 1.1.4 and 1.1.6 in file2 since nothing was checked in on some of
	  # these branches and CVS only tracks branches via tags unless they contain data.

	  # try intentionally converting non-branch tag to branch tag (tag -F -b)
	  dotest tagf-25a "${testcvs} tag -F -b regulartag file1" "T file1"
	  # try intentionally moving a branch tag to a newly created branch (tag -F -b -B)
	  dotest tagf-25b "${testcvs} tag -F -B -b -r1.1 regulartag file1" \
"T file1"
	  # try intentionally converting mixed tags to branch tags (rtag -F -b)
	  dotest tagf-26a "${testcvs} rtag -F -b regulartag first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: first-dir/file1: Not moving branch tag .regulartag. from 1\.1 to 1\.1\.0\.8\."
	  # try intentionally converting a branch to a new branch tag (rtag -F -b -B)
	  dotest tagf-26b "${testcvs} rtag -F -B -b -r1.1 regulartag first-dir" \
"${PROG} rtag: Tagging first-dir"
	  # update to our new branch
	  dotest tagf-27 "${testcvs} update -r regulartag" \
"${PROG} update: Updating \.
U file1
U file2"
	  # commit some changes and see that all rev numbers look right
	  echo changes >> file1
	  echo changes >> file2
	  dotest tagf-28 "${testcvs} ci -m changes" \
"${PROG} [a-z]*: Examining \.
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.8\.1; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.6\.1; previous revision: 1\.1
done"
	  # try intentional branch to non-branch (tag -F -B)
	  dotest tagf-29 "${testcvs} tag -F -B -r1.1 regulartag file1" \
"T file1"
	  # try non-branch to non-branch (tag -F -B)
	  dotest tagf-29a "${testcvs} tag -F -B -r br regulartag file1" \
"${PROG} tag: file1: Not moving non-branch tag .regulartag. from 1\.1 to 1\.1\.4\.1 due to .-B. option\."
	  # try mixed-branch to non-branch (rtag -F -B )
	  dotest tagf-29b "${testcvs} rtag -F -B -r br regulartag first-dir" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: first-dir/file1: Not moving non-branch tag .regulartag. from 1\.1 to 1\.1\.4\.1 due to .-B. option\."
	  # at this point, regulartag is a regular tag within
	  # file1 and file2

	  # try intentional branch to non-branch (rtag -F -B)
	  dotest tagf-30 "${testcvs} rtag -F -B -r1.1 br first-dir"  \
"${PROG} rtag: Tagging first-dir"
	  # create a branch tag so we can try to delete it.
	  dotest tagf-31 "${testcvs} rtag -b brtag first-dir"  \
"${PROG} rtag: Tagging first-dir"
	
	  # try intentinal deletion of branch tag (tag -d -B)
	  dotest tagf-32 "${testcvs} tag -d -B brtag file1" "D file1"
	  # try intentinal deletion of branch tag (rtag -d -B)
	  dotest tagf-33 "${testcvs} rtag -d -B brtag first-dir" \
"${PROG} rtag: Untagging first-dir"

	  cd ../..

	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	tag-log)
	  # Test log output for tags
	  mkdir 1; cd 1
	  dotest tag-log-init-1 "$testcvs -q co -l ."
	  mkdir first-dir
	  dotest tag-log-init-2 "$testcvs add first-dir" \
"Directory $CVSROOT_DIRNAME/first-dir added to the repository"
	  cd first-dir
	  touch file1
	  dotest tag-log-init-3 "$testcvs add file1" \
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest tag-log-init-4 "$testcvs -Q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

          dotest tag-log-1 "$testcvs -Q tag mytag file1" ''
          dotest tag-log-2 "$testcvs log -N file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
          dotest tag-log-3 "$testcvs log -N -n file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	mytag: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add
============================================================================="
          dotest tag-log-4 "$testcvs log file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	mytag: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add
============================================================================="
          dotest tag-log-5 "$testcvs log -n file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	mytag: 1\.1
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add
============================================================================="

          cd ../..
	  rm -fr 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	tag-space)
	  # Test tags with spaces in the names.
	  #
	  # Prior to releases 1.11.18 & 1.12.10, some commands used with
	  # tags with spaces in the names could hang CVS.

	  # Setup; check in first-dir/file1
	  mkdir 1; cd 1
	  dotest tag-space-init-1 "$testcvs -q co -l ."
	  mkdir first-dir
	  dotest tag-space-init-2 "$testcvs add first-dir" \
"Directory $CVSROOT_DIRNAME/first-dir added to the repository"
	  cd first-dir
	  touch file1
	  dotest tag-space-init-3 "$testcvs add file1" \
"$PROG add: scheduling file \`file1' for addition
$PROG add: use '$PROG commit' to add this file permanently"
	  dotest tag-space-init-4 "$testcvs -Q ci -m add" \
"RCS file: $CVSROOT_DIRNAME/first-dir/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  # Reportedly, the following two tags make it past WinCVS.
	  dotest_fail tag-space-1 "$testcvs tag ' spacetag '" \
"$PROG \[tag aborted\]: tag \` spacetag ' must start with a letter"
	  dotest_fail tag-space-2 "$testcvs tag 'spacetag '" \
"$PROG \[tag aborted\]: tag \`spacetag ' has non-visible graphic characters"

	  if $remote; then
	    # Verify that this isn't a client check.
	    dotest tag-space-3 "$testcvs server" \
"E $PROG \[tag aborted\]: tag \` spacetag ' must start with a letter
error  " <<EOF
Root $CVSROOT_DIRNAME
UseUnchanged
Argument --
Argument  spacetag 
Directory .
$CVSROOT_DIRNAME/first-dir
Entry /file1/1.1///
Unchanged file1
tag
EOF

	    dotest tag-space-4 "$testcvs server" \
"E $PROG \[tag aborted\]: tag \`spacetag ' has non-visible graphic characters
error  " <<EOF
Root $CVSROOT_DIRNAME
UseUnchanged
Argument --
Argument spacetag 
Directory .
$CVSROOT_DIRNAME/first-dir
Entry /file1/1.1///
Unchanged file1
tag
EOF
	  fi # $remote

	  # Any number of normal tags and branches were handled correctly.
	  dotest tag-space-5 "$testcvs -Q tag t1"
	  dotest tag-space-5b "$testcvs -Q tag t2"
	  dotest tag-space-5c "$testcvs -Q tag -b b1"

	  cd ../..
	  mkdir 2; cd 2

	  # But once a vendor branch exists, it's all over.
	  mkdir project; cd project
	  touch file1
	  dotest tag-space-init-4 \
"$testcvs -Q import -mimport second-dir VENDOR RELEASE"

	  cd ..

	  dotest_fail tag-space-6 "$testcvs -Q co -r ' spacetag ' first-dir" \
"$PROG \[checkout aborted\]: tag \` spacetag ' must start with a letter"

	  # But when any files were imported, this test hung prior to CVS
	  # versions 1.11.18 & 1.12.10.
	  dotest_fail tag-space-7 "$testcvs -Q co -r ' spacetag ' second-dir" \
"$PROG \[checkout aborted\]: tag \` spacetag ' must start with a letter"

	  if $remote; then
	    # I based the client input in the next two tests on actual input
	    # from WinCVS 1.2.
	    dotest tag-space-8 "$testcvs server" \
"E $PROG \[checkout aborted\]: tag \` spacetag ' must start with a letter
error  " <<EOF
Root $CVSROOT_DIRNAME
Argument -P
Argument -r
Argument  spacetag 
Argument first-dir
Directory .
$CVSROOT_DIRNAME
co
EOF

	    # Verify the test is not on the client side.
	    dotest tag-space-9 "$testcvs server" \
"E $PROG \[checkout aborted\]: tag \` spacetag ' must start with a letter
error  " <<EOF
Root $CVSROOT_DIRNAME
Argument -P
Argument -r
Argument  spacetag 
Argument second-dir
Directory .
$CVSROOT_DIRNAME
co
EOF
	  fi # $remote

	  dotest tag-space-10 "$testcvs -Q co second-dir"
	  cd second-dir

	  # This test would also hang.
	  dotest_fail tag-space-11 "$testcvs -Q up -r ' spacetag '" \
"$PROG \[update aborted\]: tag \` spacetag ' must start with a letter"

	  if $remote; then
	    dotest tag-space-12 "$testcvs server" \
"E $PROG \[update aborted\]: tag \` spacetag ' must start with a letter
error  " <<EOF
Root $CVSROOT_DIRNAME
Argument -r
Argument  spacetag 
Argument -u
Argument --
Directory .
$CVSROOT_DIRNAME
Unchanged file1
update
EOF
	  fi # $remote

	  # I'm skipping tests for other commands that may have had the same
	  # problem.  Hopefully, if a new issue arises, one of the above tests
	  # will catch the problem.

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -r 1 2
	  rm -rf $CVSROOT_DIRNAME/first-dir $CVSROOT_DIRNAME/second-dir
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
"${PROG} add: scheduling file .foo\.c. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rcsdiff-3 "${testcvs} commit -m rev1 foo.c" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/foo\.c,v
done
Checking in foo\.c;
${CVSROOT_DIRNAME}/first-dir/foo.c,v  <--  foo\.c
initial revision: 1\.1
done"
	  dotest rcsdiff-4 "${testcvs} tag first foo.c" "T foo\.c"
	  dotest rcsdiff-5 "${testcvs} update -p -r first foo.c" \
"===================================================================
Checking out foo\.c
RCS:  ${CVSROOT_DIRNAME}/first-dir/foo\.c,v
VERS: 1\.1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
I am the first foo, and my name is \$""Name: first \$\."

	  echo "I am the second foo, and my name is $""Name$." > foo.c
	  dotest rcsdiff-6 "${testcvs} commit -m rev2 foo.c" \
"Checking in foo\.c;
${CVSROOT_DIRNAME}/first-dir/foo\.c,v  <--  foo\.c
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest rcsdiff-7 "${testcvs} tag second foo.c" "T foo\.c"
	  dotest rcsdiff-8 "${testcvs} update -p -r second foo.c" \
"===================================================================
Checking out foo\.c
RCS:  ${CVSROOT_DIRNAME}/first-dir/foo\.c,v
VERS: 1\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
I am the second foo, and my name is \$""Name: second \$\."

	dotest_fail rcsdiff-9 "${testcvs} diff -r first -r second" \
"${PROG} diff: Diffing \.
Index: foo\.c
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/foo\.c,v
retrieving revision 1\.1
retrieving revision 1\.2
diff -r1\.1 -r1\.2
1c1
< I am the first foo, and my name is \$""Name:  \$\.
---
> I am the second foo, and my name is \$""Name:  \$\."

	  echo "I am the once and future foo, and my name is $""Name$." > foo.c
	  dotest_fail rcsdiff-10 "${testcvs} diff -r first" \
"${PROG} diff: Diffing \.
Index: foo\.c
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/foo\.c,v
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
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rcslib-diffrgx-2 "${testcvs} -q ci -m '' rgx.c" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/rgx\.c,v
done
Checking in rgx\.c;
${CVSROOT_DIRNAME}/first-dir/rgx\.c,v  <--  rgx\.c
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
	  dotest_fail rcslib-diffrgx-3 "${testcvs} diff -c -F'.* (' rgx.c" \
"Index: rgx\.c
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/rgx\.c,v
retrieving revision 1\.1
diff -c -F \.\* ( -r1\.1 rgx\.c
\*\*\* rgx\.c	${RFCDATE}	1\.1
--- rgx\.c	${RFCDATE}
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
"Directory ${CVSROOT_DIRNAME}.*/first-dir added to the repository"
	  cd ..; rm -r 1

	  dotest rcslib-merge-3 "${testcvs} -q co first-dir" ""
	  cd first-dir

	  echo '$''Revision$' > file1
	  echo '2' >> file1
	  echo '3' >> file1
	  dotest rcslib-merge-4 "${testcvs} -q add file1" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest rcslib-merge-5 "${testcvs} -q commit -m '' file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  sed -e 's/2/two/' file1 > f; mv f file1
	  dotest rcslib-merge-6 "${testcvs} -q commit -m '' file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest rcslib-merge-7 "${testcvs} -q tag -b -r 1.1 patch1" "T file1"
	  dotest rcslib-merge-8 "${testcvs} -q update -r patch1" "[UP] file1"
	  dotest rcslib-merge-9 "${testcvs} -q status" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest rcslib-merge-12 "${testcvs} -q update -kv -j1.2" \
"U file1
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  # Test behavior of symlinks in the repository.
	  if test -n "$remotehost"; then
	    # Create the link on the remote system.  This is because Cygwin's
	    # Windows support creates *.lnk files for Windows.  When creating
	    # these in an SMB share from UNIX, these links won't work from the
	    # UNIX side.
	    dotest rcslib-symlink-1remotehost "${CVS_RSH} $remotehost 'ln -s file1,v ${CVSROOT_DIRNAME}/first-dir/file2,v'"
	  else
	    dotest rcslib-symlink-1 "ln -s file1,v ${CVSROOT_DIRNAME}/first-dir/file2,v"
	  fi
	  dotest rcslib-symlink-2 "${testcvs} update file2" "U file2"
	  echo "This is a change" >> file2
	  dotest rcslib-symlink-3 "${testcvs} ci -m because file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file2
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # Switch as for rcslib-symlink-1
	  if test -n "$remotehost"; then
	    dotest rcslib-symlink-4 "$CVS_RSH $remotehost 'ls -l $CVSROOT_DIRNAME/first-dir/file2,v'" \
".*$CVSROOT_DIRNAME/first-dir/file2,v -> file1,v"
	  else
	    dotest rcslib-symlink-4 "ls -l $CVSROOT_DIRNAME/first-dir/file2,v" \
".*$CVSROOT_DIRNAME/first-dir/file2,v -> file1,v"
	  fi

	  # CVS was failing to check both the symlink and the file
	  # for timestamp changes for a while.  Test that.
	  rm file1
	  dotest rcslib-symlink-3a "${testcvs} -q up file1" \
"${PROG} update: warning: file1 was lost
U file1"
	  echo "This is a change" >> file1
	  dotest rcslib-symlink-3b "${testcvs} ci -m because file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.[0-9]*; previous revision: 1\.1\.2\.[0-9]*
done"
	  dotest rcslib-symlink-3c "${testcvs} update file2" "[UP] file2"

	  echo some new text >file3
	  dotest rcslib-symlink-3d "${testcvs} -Q add file3" ''
	  dotest rcslib-symlink-3e "${testcvs} -Q ci -mtest file3" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  rm -f ${CVSROOT_DIRNAME}/first-dir/file2,v
	  # As for rcslib-symlink-1
	  if test -n "$remotehost"; then
	    dotest rcslib-symlink-3f "$CVS_RSH $remotehost 'ln -s Attic/file3,v ${CVSROOT_DIRNAME}/first-dir/file2,v'"
	  else
	    dotest rcslib-symlink-3f "ln -s Attic/file3,v ${CVSROOT_DIRNAME}/first-dir/file2,v"
	  fi

	  dotest rcslib-symlink-3g "${testcvs} update file2" "U file2"

	  # restore the link to file1 for the following tests
	  dotest rcslib-symlink-3i "${testcvs} -Q rm -f file3" ''
	  dotest rcslib-symlink-3j "${testcvs} -Q ci -mwhatever file3" \
"Removing file3;
${CVSROOT_DIRNAME}/first-dir/Attic/file3,v  <--  file3
new revision: delete; previous revision: 1\.1\.2\.1
done"
	  rm -f ${CVSROOT_DIRNAME}/first-dir/file2,v
	  rm -f ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
	  # As for rcslib-symlink-1
	  if test -n "$remotehost"; then
	    dotest rcslib-symlink-3h "$CVS_RSH $remotehost 'ln -s file1,v ${CVSROOT_DIRNAME}/first-dir/file2,v'"
	  else
	    dotest rcslib-symlink-3h "ln -s file1,v ${CVSROOT_DIRNAME}/first-dir/file2,v"
	  fi

	  # Test 5 reveals a problem with having symlinks in the
	  # repository.  CVS will try to tag both of the files
	  # separately.  After processing one, it will do the same
	  # operation to the other, which is actually the same file,
	  # so the tag will already be there.  FIXME: do we bother
	  # changing operations to notice cases like this?  This
	  # strikes me as a difficult problem.  -Noel
	  dotest rcslib-symlink-5 "${testcvs} tag the_tag" \
"${PROG} tag: Tagging .
T file1
W file2 : the_tag already exists on version 1.1.2.3 : NOT MOVING tag to version 1.1.2.1"
	  # As for rcslib-symlink-1
	  if test -n "$remotehost"; then
	    dotest rcslib-symlink-6 "$CVS_RSH $remotehost 'ls -l $CVSROOT_DIRNAME/first-dir/file2,v'" \
".*$CVSROOT_DIRNAME/first-dir/file2,v -> file1,v"
	  else
	    dotest rcslib-symlink-6 "ls -l $CVSROOT_DIRNAME/first-dir/file2,v" \
".*$CVSROOT_DIRNAME/first-dir/file2,v -> file1,v"
	  fi

	  # Symlinks tend to interact poorly with the Attic.
	  cd ..
	  mkdir 2; cd 2
	  dotest rcslib-symlink-7 "${testcvs} -q co first-dir" \
"U first-dir/file1
U first-dir/file2"
	  cd first-dir
	  dotest rcslib-symlink-8 "${testcvs} rm -f file2" \
"${PROG} remove: scheduling .file2. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest rcslib-symlink-9 "${testcvs} -q ci -m rm-it" \
"Removing file2;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file2
new revision: delete; previous revision: 1\.2
done"
	  # OK, why this message happens twice is relatively clear
	  # (the check_* and rtag_* calls to start_recursion).
	  # Why it happens a third time I didn't try to find out.
	  dotest rcslib-symlink-10 \
"${testcvs} -q rtag -b -r the_tag brtag first-dir" \
"${PROG} rtag: could not read RCS file for file2
${PROG} rtag: could not read RCS file for first-dir/file2
${PROG} rtag: could not read RCS file for first-dir/file2"

	  # Restore file1 for the next test.
	  dotest rcslib-long-symlink-init-1 "$testcvs -Q up -A"
	  dotest rcslib-long-symlink-init-2 "$testcvs -Q add file1"
	  dotest rcslib-long-symlink-init-3 "$testcvs -Q ci -mback" \
"Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"

	  cd ../..  # $TESTDIR

	  # CVS has a hard-coded default link path size of 127 characters.
	  # Make sure it knows how to exceed that.
	  longpath=$CVSROOT_DIRNAME
	  count=0
	  while test $count -lt 10; do
	    count=`expr $count + 1`
	    longpath=$longpath/123456789012345678901234567890
	    mkdir $longpath
	  done
	  cp $CVSROOT_DIRNAME/first-dir/file1,v $longpath
	  mkdir $CVSROOT_DIRNAME/second-dir

	  # Switch as for rcslib-symlink-1
	  if test -n "$remotehost"; then
	    dotest rcslib-long-symlink-1rh \
"$CVS_RSH $remotehost 'ln -s $longpath/file1,v $CVSROOT_DIRNAME/second-dir/fileX,v'"
	  else
	    dotest rcslib-long-symlink-1 \
"ln -s $longpath/file1,v $CVSROOT_DIRNAME/second-dir/fileX,v"
	  fi

	  dotest rcslib-long-symlink-2 "$testcvs co second-dir" \
"$PROG checkout: Updating second-dir
U second-dir/fileX"

	  cd second-dir
	  echo change-it >>fileX

	  # Writes actually cause symlinks to be resolved.
	  dotest rcslib-long-symlink-3 "$testcvs -q ci -mwrite-it" \
"Checking in fileX;
$CVSROOT_DIRNAME/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/123456789012345678901234567890/file1,v  <--  fileX
new revision: 1\.5; previous revision: 1\.4
done"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  # Must remove the symlink first.  Samba doesn't appear to show
	  # broken symlink across the SMB share, and rm -rf by itself
	  # will remove file1,v first and leave file2,v a broken link and the
	  # rm -rf will fail since it doesn't find file2,v and it still gets
	  # directory not empty errors removing cvsroot/first-dir.
	  #
	  # I'm not sure why I need to do this on $remotehost.  The rm above
	  # rcslib-symlink-3j works fine, but the next one doesn't unless run
	  # remotely under Cygwin and using a TESTDIR on a Samba share.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost \
"rm -f $CVSROOT_DIRNAME/first-dir/file2,v $CVSROOT_DIRNAME/second-dir/fileX,v"
	  fi
	  rm -rf $CVSROOT_DIRNAME/first-dir $CVSROOT_DIRNAME/second-dir \
		 $CVSROOT_DIRNAME/123456789012345678901234567890
	  rm -r first-dir second-dir 2
	  ;;

	multibranch)
	  # Test the ability to have several branchpoints coming off the
	  # same revision.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest multibranch-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 1:trunk-1 >file1
	  dotest multibranch-2 "${testcvs} add file1" \
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest_lit multibranch-3 "${testcvs} -q ci -m add-it" <<HERE
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1.1
done
HERE
	  dotest multibranch-4 "${testcvs} tag -b br1" \
"${PROG} tag: Tagging \.
T file1"
	  dotest multibranch-5 "${testcvs} tag -b br2" \
"${PROG} tag: Tagging \.
T file1"
	  dotest multibranch-6 "$testcvs -q update -r br1" '[UP] file1'
	  echo on-br1 >file1
	  dotest multibranch-7 "${testcvs} -q ci -m modify-on-br1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest multibranch-8 "${testcvs} -q update -r br2" '[UP] file1'
	  echo br2 adds a line >>file1
	  dotest multibranch-9 "${testcvs} -q ci -m modify-on-br2" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"
	  dotest multibranch-10 "${testcvs} -q update -r br1" '[UP] file1'
	  dotest multibranch-11 "cat file1" 'on-br1'
	  dotest multibranch-12 "${testcvs} -q update -r br2" '[UP] file1'
	  dotest multibranch-13 "cat file1" '1:trunk-1
br2 adds a line'

	  dotest multibranch-14 "${testcvs} log file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  if $keep; then
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
		# importc -- bunch o' files in bunch o' directories
		# modules3
		# mflag -- various -m messages
		# ignore  -- import and cvsignore
		# binwrap -- import and -k wrappers
		# info -- imports which are rejected by verifymsg
		# head -- intended to test vendor branches and HEAD,
		#   although it doesn't really do it yet.
		# import-CVS -- refuse to import directories named "CVS".
		# import-quirks -- short tests of import quirks.

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

		dotest_sort import-96 \
"${testcvs} import -m first-import first-dir vendor-branch junk-1_0" \
"

I first-dir/RCS
N first-dir/imported-f1
N first-dir/imported-f2
N first-dir/imported-f3
N first-dir/imported-f4
No conflicts created by this import"

		dotest import-96.5 "cmp ../imported-f2-orig.tmp imported-f2" ''

		cd ..

		# co
		dotest import-97 "${testcvs} -q co first-dir" \
"U first-dir/imported-f1
U first-dir/imported-f2
U first-dir/imported-f3
U first-dir/imported-f4"

		cd first-dir

		for i in 1 2 3 4 ; do
		  dotest import-98-$i "test -f imported-f$i" ''
		done
		dotest_fail import-98.5 "test -d RCS" ''

		# remove
		rm imported-f1
		dotest import-99 "${testcvs} rm imported-f1" \
"${PROG}"' remove: scheduling `imported-f1'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove this file permanently'

		# change
		echo local-change >> imported-f2

		# commit
		dotest import-100 "${testcvs} ci -m local-changes" \
"${PROG} [a-z]*: Examining .
Removing imported-f1;
${CVSROOT_DIRNAME}/first-dir/imported-f1,v  <--  imported-f1
new revision: delete; previous revision: 1\.1\.1\.1
done
Checking in imported-f2;
${CVSROOT_DIRNAME}/first-dir/imported-f2,v  <--  imported-f2
new revision: 1\.2; previous revision: 1\.1
done"

		# log
		dotest import-101 "${testcvs} log imported-f1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/imported-f1,v
Working file: imported-f1
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	junk-1_0: 1\.1\.1\.1
	vendor-branch: 1\.1\.1
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: dead;  lines: ${PLUS}0 -0
local-changes
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
branches:  1\.1\.1;
Initial revision
----------------------------
revision 1\.1\.1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}0 -0
first-import
============================================================================="

		# update into the vendor branch.
		dotest import-102 "$testcvs update -rvendor-branch" \
"$PROG update: Updating .
U imported-f1
[UP] imported-f2
[UP] imported-f3
[UP] imported-f4"

		# remove file4 on the vendor branch
		rm imported-f4
		dotest import-103 "${testcvs} rm imported-f4" \
"${PROG}"' remove: scheduling `imported-f4'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove this file permanently'

		# commit
		dotest import-104 \
"${testcvs} ci -m vendor-removed imported-f4" \
"Removing imported-f4;
${CVSROOT_DIRNAME}/first-dir/imported-f4,v  <--  imported-f4
new revision: delete; previous revision: 1\.1\.1\.1
done"

		# update to main line
		dotest import-105 "$testcvs -q update -A" \
"$PROG update: imported-f1 is no longer in the repository
[UP] imported-f2
[UP] imported-f3"

		# second import - file4 deliberately unchanged
		cd ../import-dir
		for i in 1 2 3 ; do
		  echo rev 2 of file $i >> imported-f"$i"
		done
		cp imported-f2 ../imported-f2-orig.tmp

		dotest_sort import-106 \
"${testcvs} import -m second-import first-dir vendor-branch junk-2_0" \
"


 ${PROG} checkout -j<prev_rel_tag> -jjunk-2_0 first-dir
2 conflicts created by this import.
C first-dir/imported-f1
C first-dir/imported-f2
I first-dir/RCS
U first-dir/imported-f3
U first-dir/imported-f4
Use the following command to help the merge:"

		dotest import-106.5 "cmp ../imported-f2-orig.tmp imported-f2" \
''

		cd ..

		rm imported-f2-orig.tmp

		# co
		dotest import-107 "${testcvs} co first-dir" \
"${PROG} checkout: Updating first-dir
[UP] first-dir/imported-f3
[UP] first-dir/imported-f4"

		cd first-dir

		dotest_fail import-108 "test -f imported-f1" ''

		for i in 2 3 ; do
		  dotest import-109-$i "test -f imported-f$i" ''
		done

		# check vendor branch for file4
		dotest import-110 "$testcvs -q update -rvendor-branch" \
'U imported-f1
[UP] imported-f2
[UP] imported-f3
[UP] imported-f4'

		dotest import-111 "test -f imported-f4" ''

		# update to main line
		dotest import-112 "$testcvs -q update -A" \
"$PROG update: imported-f1 is no longer in the repository
[UP] imported-f2
[UP] imported-f3
[UP] imported-f4"

		cd ..

		dotest import-113 \
"${testcvs} -q co -jjunk-1_0 -jjunk-2_0 first-dir" \
"${PROG} checkout: file first-dir/imported-f1 does not exist, but is present in revision junk-2_0
RCS file: ${CVSROOT_DIRNAME}/first-dir/imported-f2,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.2
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.2 into imported-f2
rcsmerge: warning: conflicts during merge
first-dir/imported-f3 already contains the differences between 1\.1\.1\.1 and 1\.1\.1\.2
first-dir/imported-f4 already contains the differences between 1\.1\.1\.1 and 1\.1\.1\.3"

		cd first-dir

		dotest_fail import-114 "test -f imported-f1" ''

		for i in 2 3 ; do
		  dotest import-115-$i "test -f imported-f$i" ''
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
	  dotest_sort importb-1 \
"${testcvs} import -m add first-dir openmunger openmunger-1_0" \
"

N first-dir/file1
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
	  # We use an explicit -d option to test that it is reflected
	  # in the suggested checkout.
	  dotest_sort importb-2 \
"${testcvs} -d ${CVSROOT} import -m add -b 1.1.3 first-dir freemunger freemunger-1_0" \
"


 ${PROG} -d ${CVSROOT} checkout -j<prev_rel_tag> -jfreemunger-1_0 first-dir
2 conflicts created by this import.
C first-dir/file1
C first-dir/file2
Use the following command to help the merge:"
	  cd ..
	  rm -r imp-dir

	  # Now a test of main branch import (into second-dir, not first-dir).
	  mkdir imp-dir
	  cd imp-dir
	  echo 'my own stuff' >mine1.c
	  echo 'my own stuff' >mine2.c
	  dotest_fail importb-3 \
"${testcvs} import -m add -b 1 second-dir dummy really_dumb_y" \
"$PROG \[import aborted\]: Only numeric branch specifications with two dots are
supported by import, not \`1'\.  For example: \`1\.1\.1'\."
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
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	importc)
	  # Test importing a bunch o' files in a bunch o' directories.
	  # Also the -d option.
	  mkdir 1; cd 1
	  mkdir adir bdir cdir
	  mkdir adir/sub1 adir/sub2
	  mkdir adir/sub1/ssdir
	  mkdir bdir/subdir
	  touch adir/sub1/file1 adir/sub2/file2 adir/sub1/ssdir/ssfile
	  touch -t 197107040343 bdir/subdir/file1
	  touch -t 203412251801 cdir/cfile
	  dotest_sort importc-1 \
"${testcvs} import -d -m import-it first-dir vendor release" \
"

N first-dir/adir/sub1/file1
N first-dir/adir/sub1/ssdir/ssfile
N first-dir/adir/sub2/file2
N first-dir/bdir/subdir/file1
N first-dir/cdir/cfile
No conflicts created by this import
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/adir
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/adir/sub1
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/adir/sub1/ssdir
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/adir/sub2
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/bdir
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/bdir/subdir
${PROG} import: Importing ${CVSROOT_DIRNAME}/first-dir/cdir"
	  cd ..
	  mkdir 2; cd 2
	  dotest importc-2 "${testcvs} -q co first-dir" \
"U first-dir/adir/sub1/file1
U first-dir/adir/sub1/ssdir/ssfile
U first-dir/adir/sub2/file2
U first-dir/bdir/subdir/file1
U first-dir/cdir/cfile"
	  cd first-dir
	  dotest importc-3 "${testcvs} update adir/sub1" \
"${PROG} update: Updating adir/sub1
${PROG} update: Updating adir/sub1/ssdir"
	  dotest importc-4 "${testcvs} update adir/sub1 bdir/subdir" \
"${PROG} update: Updating adir/sub1
${PROG} update: Updating adir/sub1/ssdir
${PROG} update: Updating bdir/subdir"

	  echo modify >>cdir/cfile
	  dotest importc-5 \
"${testcvs} -q rtag -b -r release wip_test first-dir" ""
	  dotest importc-6 "$testcvs -q update -r wip_test" \
'U adir/sub1/file1
U adir/sub1/ssdir/ssfile
U adir/sub2/file2
U bdir/subdir/file1
M cdir/cfile'

	  # This used to fail in local mode
	  dotest importc-7 "${testcvs} -q ci -m modify -r wip_test" \
"Checking in cdir/cfile;
${CVSROOT_DIRNAME}/first-dir/cdir/cfile,v  <--  cfile
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done"

	  # TODO: should also be testing "import -d" when we update
	  # an existing file.
	  dotest importc-8 "${testcvs} -q log cdir/cfile" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/cdir/cfile,v
Working file: cdir/cfile
head: 1\.1
branch: 1\.1\.1
locks: strict
access list:
symbolic names:
	wip_test: 1\.1\.1\.1\.0\.2
	release: 1\.1\.1\.1
	vendor: 1\.1\.1
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.1
date: 2034/12/2[4-6] [0-9][0-9]:01:[0-9][0-9];  author: ${username};  state: Exp;
branches:  1\.1\.1;
Initial revision
----------------------------
revision 1\.1\.1\.1
date: 2034/12/2[4-6] [0-9][0-9]:01:[0-9][0-9];  author: ${username};  state: Exp;  lines: ${PLUS}0 -0
branches:  1\.1\.1\.1\.2;
import-it
----------------------------
revision 1\.1\.1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
modify
============================================================================="

	  dotest importc-9 "${testcvs} -q log bdir/subdir/file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/bdir/subdir/file1,v
Working file: bdir/subdir/file1
head: 1\.1
branch: 1\.1\.1
locks: strict
access list:
symbolic names:
	wip_test: 1\.1\.1\.1\.0\.2
	release: 1\.1\.1\.1
	vendor: 1\.1\.1
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: 1971/07/0[3-5] [0-9][0-9]:43:[0-9][0-9];  author: ${username};  state: Exp;
branches:  1\.1\.1;
Initial revision
----------------------------
revision 1\.1\.1\.1
date: 1971/07/0[3-5] [0-9][0-9]:43:[0-9][0-9];  author: ${username};  state: Exp;  lines: ${PLUS}0 -0
import-it
============================================================================="
	  cd ..

	  # Now tests of absolute pathnames and .. as repository directory.
	  cd ../1
	  dotest_fail importc-10 \
"${testcvs} import -m imp ../other vendor release2" \
"${PROG} \[[a-z]* aborted\]: directory \.\./other not relative within the repository"
	  dotest_fail importc-11 \
"${testcvs} import -m imp ${TESTDIR}/other vendor release3" \
"${PROG} \[[a-z]* aborted\]: directory ${TESTDIR}/other not relative within the repository"
	  dotest_fail importc-12 "test -d ${TESTDIR}/other" ""
	  cd ..

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	import-CVS)
	  mkdir import-CVS
	  cd import-CVS
	  touch file1 file2 file3
	  dotest_fail import-CVS-1 "$testcvs import -mimport CVS vtag rtag" \
"$PROG import: The word \`CVS' is reserved by CVS and may not be used
$PROG \[import aborted\]: as a directory in a path or as a file name\."
	  dotest_fail import-CVS-1b \
"$testcvs import -mimport CVS-/CVS vtag rtag" \
"$PROG import: The word \`CVS' is reserved by CVS and may not be used
$PROG \[import aborted\]: as a directory in a path or as a file name\."
	  mkdir sdir
	  mkdir sdir/CVS
	  touch CVS sdir/CVS/file4 sdir/CVS/file5 sdir/file6 sdir/file7
	  # Calling the imported directory import-CVS is dual purpose in the
	  # following test.  It makes sure the path test which matched above
	  # wasn't too strict.
	  dotest_sort import-CVS-2 \
"$testcvs import -I! -mimport import-CVS vtag rtag" \
"

I import-CVS/CVS
I import-CVS/sdir/CVS
N import-CVS/file1
N import-CVS/file2
N import-CVS/file3
N import-CVS/sdir/file6
N import-CVS/sdir/file7
No conflicts created by this import
$PROG import: Importing $CVSROOT_DIRNAME/import-CVS/sdir"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  rm -r import-CVS
	  rm -rf $CVSROOT_DIRNAME/import-CVS
	  ;;



	import-quirks)
	  # Short tests of quirky import behavior.
	  #
	  # For a list of other import tests with short descriptions, see the
	  # comment header of the "import" test.
	  mkdir import-quirks
	  cd import-quirks
	  touch file1 file2 file3

	  # CVS prior to 1.11.18 and 1.12.10 used to happily import to
	  # "branch 1.1", creating RCS archives with revisions like,
	  # "1.1..1".  That double-dot is *not* a typo.
	  dotest_fail import-quirks-1 \
"$testcvs import -b1.1. -mbad-bad-bad import-quirks VB RT" \
"$PROG \[import aborted\]: Only numeric branch specifications with two dots are
supported by import, not \`1\.1\.'\.  For example: \`1\.1\.1'\."

	  dotest_fail import-quirks-2 \
"$testcvs import -b1.1.1.. -mbad-bad-bad import-quirks VB RT" \
"$PROG \[import aborted\]: Only numeric branch specifications with two dots are
supported by import, not \`1\.1\.1\.\.'\.  For example: \`1\.1\.1'\."

	  # Try a few odd numbers.  This is hardly comprehensive.
	  dotest_sort import-quirks-2 \
"$testcvs import -b10.10.101 -mthis-ones-ok import-quirks-2 VB RT" \
"

N import-quirks-2/file1
N import-quirks-2/file2
N import-quirks-2/file3
No conflicts created by this import"

	  dotest_sort import-quirks-3 \
"$testcvs import -b2345678901.2345678901.2345678901 -mthis-ones-ok import-quirks-3 VB RT" \
"

N import-quirks-3/file1
N import-quirks-3/file2
N import-quirks-3/file3
No conflicts created by this import"

	  dotest_sort import-quirks-4 \
"$testcvs import -b1.1.2 -mthis-ones-ok import-quirks-4 VB RT" \
"

N import-quirks-4/file1
N import-quirks-4/file2
N import-quirks-4/file3
No conflicts created by this import"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  rm -r import-quirks
	  rm -rf $CVSROOT_DIRNAME/import-quirks-2 \
		 $CVSROOT_DIRNAME/import-quirks-3 \
		 $CVSROOT_DIRNAME/import-quirks-4
	  ;;



	import-after-initial)
	  # Properly handle the case in which the first version of a
	  # file is created by a regular cvs add and commit, and there
	  # is a subsequent cvs import of the same file.  cvs update with
	  # a date tag must resort to searching the vendor branch only if
	  # the initial version of the file was created at the same time
	  # as the initial version on the vendor branch.

	  mkdir 1; cd 1
	  module=x

	  echo > unused-file

	  # Create the module.
	  dotest import-after-initial-1 \
	    "$testcvs -Q import -m. $module X Y" ''

	  file=m
	  # Check it out and add a file.
	  dotest import-after-initial-2 "$testcvs -Q co $module" ''
	  cd $module
	  echo original > $file
	  dotest import-after-initial-3 "${testcvs} -Q add $file" ""
	  dotest import-after-initial-4 "${testcvs} -Q ci -m. $file" \
"RCS file: ${CVSROOT_DIRNAME}/$module/$file,v
done
Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
initial revision: 1\.1
done"

	  # Delay a little so the following import isn't done in the same
	  # second as the preceding commit.
	  sleep 2

	  # Do the first import of $file *after* $file already has an
	  # initial version.
	  mkdir sub
	  cd sub
	  echo newer-via-import > $file
	  dotest import-after-initial-5 \
	    "$testcvs -Q import -m. $module X Y2" ''
	  cd ..

	  # Sleep a second so we're sure to be after the second of the import.
	  sleep 1

	  dotest import-after-initial-6 \
	    "$testcvs -Q update -p -D now $file" 'original'

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

        branch-after-import)
	  # Test branching after an import via both cvs tag -b and
	  # cvs add to verify that the HEAD remains at 1.1.1.1
	  # This was a FreeBSD bug documented at the URL:
	  # http://www.freebsd.org/cgi/query-pr.cgi?pr=4033

	  mkdir branch-after-import
	  cd branch-after-import

	  # OK, first we get some sources from the NetMunger project,
	  # and import them into the 1.1.1 vendor branch.
	  mkdir imp-dir
	  cd imp-dir
	  echo 'OpenMunger sources' >file1
	  echo 'OpenMunger sources' >file2
	  dotest_sort branch-after-import-1 \
"${testcvs} import -m add first-dir openmunger openmunger-1_0" \
'

N first-dir/file1
N first-dir/file2
No conflicts created by this import'
	  cd ..

	  # Next checkout the new module
	  dotest branch-after-import-2 \
"${testcvs} -q co first-dir" \
'U first-dir/file1
U first-dir/file2'
	  cd first-dir
	  # Branch tag the file1 and cvs add file2,
	  # the branch should remain the same in both cases
	  # such that a new import will not require a conflict
	  # resolution.
	  dotest branch-after-import-3 \
"${testcvs} tag -b TESTTOTRON file1" \
'T file1'
	  dotest branch-after-import-4 \
"$testcvs -q update -r TESTTOTRON" \
"[UP] file1
$PROG update: file2 is no longer in the repository"

	  cp ../imp-dir/file2 .
	  dotest branch-after-import-5 \
"${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition on branch .TESTTOTRON.
${PROG} add: use .${PROG} commit. to add this file permanently"

	  dotest branch-after-import-6 \
"${testcvs} commit -m cvs-add file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.1\.1\.2\.2; previous revision: 1\.1\.1\.1\.2\.1
done"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -r branch-after-import
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
	  # See also rmadd2, which tests -j cases not involving branches
	  #   (e.g. undoing a commit)
	  # See also join3, which tests some cases involving the greatest
	  # common ancestor.  Here is a list of tests according to branch
	  # topology:
	  #
	  # --->bp---->trunk          too many to mention
	  #     \----->branch
	  #
	  #     /----->branch1
	  # --->bp---->trunk          multibranch, multibranch2
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
	  #   7) File not added between T1 and T2, added on main line.
	  #      Nothing should happen.
	  #   8) File not modified between T1 and T2, removed on main line.
	  #      Nothing should happen.
	  #   9) File modified between T1 and T2, removed on main line.
	  #      Conflict.
	  #  10) File was never on branch, removed on main line.
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
	  #   9) File modified on branch, removed on main line.
	  #      Conflict.
	  #  10) File was never on branch, removed on main line.
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
	  echo 'first revision of file9' > file9
	  dotest join-2 "${testcvs} add file3 file4 file6 file8 file9" \
"${PROG}"' add: scheduling file `file3'\'' for addition
'"${PROG}"' add: scheduling file `file4'\'' for addition
'"${PROG}"' add: scheduling file `file6'\'' for addition
'"${PROG}"' add: scheduling file `file8'\'' for addition
'"${PROG}"' add: scheduling file `file9'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'

	  dotest join-3 "${testcvs} -q commit -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file6,v
done
Checking in file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file8,v
done
Checking in file8;
${CVSROOT_DIRNAME}/first-dir/file8,v  <--  file8
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file9,v
done
Checking in file9;
${CVSROOT_DIRNAME}/first-dir/file9,v  <--  file9
initial revision: 1\.1
done"

	  # Make a branch.
	  dotest join-4 "${testcvs} -q tag -b branch ." \
'T file3
T file4
T file6
T file8
T file9'

	  # Add file2, file7, and file10, modify file4, and remove
	  # file6, file8, and file9.
	  echo 'first revision of file2' > file2
	  echo 'second revision of file4' > file4
	  echo 'first revision of file7' > file7
	  rm file6 file8 file9
	  echo 'first revision of file10' > file10
	  dotest join-5 "${testcvs} add file2 file7 file10" \
"${PROG}"' add: scheduling file `file2'\'' for addition
'"${PROG}"' add: scheduling file `file7'\'' for addition
'"${PROG}"' add: scheduling file `file10'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'
	  dotest join-6 "${testcvs} rm file6 file8 file9" \
"${PROG}"' remove: scheduling `file6'\'' for removal
'"${PROG}"' remove: scheduling `file8'\'' for removal
'"${PROG}"' remove: scheduling `file9'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove these files permanently'
	  dotest join-7 "${testcvs} -q ci -mx ." \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file10,v
done
Checking in file10;
${CVSROOT_DIRNAME}/first-dir/file10,v  <--  file10
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.2; previous revision: 1\.1
done
Removing file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
new revision: delete; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file7,v
done
Checking in file7;
${CVSROOT_DIRNAME}/first-dir/file7,v  <--  file7
initial revision: 1\.1
done
Removing file8;
${CVSROOT_DIRNAME}/first-dir/file8,v  <--  file8
new revision: delete; previous revision: 1\.1
done
Removing file9;
${CVSROOT_DIRNAME}/first-dir/file9,v  <--  file9
new revision: delete; previous revision: 1\.1
done"

	  # Remove file10
	  dotest join-7a "${testcvs} rm -f file10" \
"${PROG}"' remove: scheduling `file10'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove this file permanently'
	  dotest join-7b "${testcvs} -q ci -mx ." \
"Removing file10;
${CVSROOT_DIRNAME}/first-dir/file10,v  <--  file10
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
U first-dir/file8
U first-dir/file9'

	  cd first-dir

	  # Modify the files on the branch, so that T1 is not an
	  # ancestor of the main line, and add file5
	  echo 'first branch revision of file3' > file3
	  echo 'first branch revision of file4' > file4
	  echo 'first branch revision of file5' > file5
	  echo 'first branch revision of file6' > file6
	  echo 'first branch revision of file9' > file9
	  dotest join-9 "${testcvs} add file5" \
"${PROG}"' add: scheduling file `file5'\'' for addition on branch `branch'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest join-10 "${testcvs} -q ci -mx ." \
"Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file5,v
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/Attic/file5,v  <--  file5
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file6;
${CVSROOT_DIRNAME}/first-dir/Attic/file6,v  <--  file6
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file9;
${CVSROOT_DIRNAME}/first-dir/Attic/file9,v  <--  file9
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Tag the current revisions on the branch.
	  dotest join-11 "${testcvs} -q tag T1 ." \
'T file3
T file4
T file5
T file6
T file8
T file9'

	  # Add file1 and file2, modify file9, and remove the other files.
	  echo 'first branch revision of file1' > file1
	  echo 'first branch revision of file2' > file2
	  echo 'second branch revision of file9' > file9
	  rm file3 file4 file5 file6
	  dotest join-12 "${testcvs} add file1 file2" \
"${PROG}"' add: scheduling file `file1'\'' for addition on branch `branch'\''
'"${PROG}"' add: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'
	  dotest join-13 "${testcvs} rm file3 file4 file5 file6" \
"${PROG}"' remove: scheduling `file3'\'' for removal
'"${PROG}"' remove: scheduling `file4'\'' for removal
'"${PROG}"' remove: scheduling `file5'\'' for removal
'"${PROG}"' remove: scheduling `file6'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove these files permanently'
	  dotest join-14 "${testcvs} -q ci -mx ." \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/Attic/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done
Removing file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file5;
${CVSROOT_DIRNAME}/first-dir/Attic/file5,v  <--  file5
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file6;
${CVSROOT_DIRNAME}/first-dir/Attic/file6,v  <--  file6
new revision: delete; previous revision: 1\.1\.2\.1
done
Checking in file9;
${CVSROOT_DIRNAME}/first-dir/Attic/file9,v  <--  file9
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # Tag the current revisions on the branch.
	  dotest join-15 "${testcvs} -q tag T2 ." \
'T file1
T file2
T file8
T file9'

	  # Do a checkout with a merge.
	  cd ../..
	  mkdir 3
	  cd 3
	  dotest join-16 "$testcvs -q co -jT1 -jT2 first-dir" \
"U first-dir/file1
U first-dir/file2
$PROG checkout: file first-dir/file2 exists, but has been added in revision T2
U first-dir/file3
$PROG checkout: scheduling first-dir/file3 for removal
U first-dir/file4
$PROG checkout: file first-dir/file4 has been removed in revision T2, but the destination is incompatibly modified
C first-dir/file4
U first-dir/file7
$PROG checkout: file first-dir/file9 does not exist, but is present in revision T2"

	  # Verify that the right changes have been scheduled.
	  cd first-dir
	  dotest_fail join-17 "$testcvs -q update" \
'A file1
R file3
C file4'

	  # Modify file4 locally, and do an update with a merge.
	  cd ../../1/first-dir
	  echo 'third revision of file4' > file4
	  dotest join-18 "$testcvs -q update -jT1 -jT2 ." \
"U file1
$PROG update: file file2 exists, but has been added in revision T2
$PROG update: scheduling file3 for removal
M file4
$PROG update: file file4 has been removed in revision T2, but the destination is incompatibly modified
C file4
$PROG update: file file9 does not exist, but is present in revision T2"

	  # Verify that the right changes have been scheduled.
	  dotest_fail join-19 "$testcvs -q update" \
'A file1
R file3
C file4'

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
	  dotest join-20 "$testcvs -q co -jbranch first-dir" \
"U first-dir/file1
U first-dir/file2
RCS file: $CVSROOT_DIRNAME/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
Merging differences between 1\.1 and 1\.1\.2\.2 into file2
U first-dir/file3
$PROG checkout: scheduling first-dir/file3 for removal
U first-dir/file4
$PROG checkout: file first-dir/file4 has been removed in revision branch, but the destination is incompatibly modified
C first-dir/file4
U first-dir/file7
$PROG checkout: file first-dir/file9 does not exist, but is present in revision branch"

	  # Verify that the right changes have been scheduled.
	  # The M file2 line is a bug; see above join-20.
	  cd first-dir
	  dotest_fail join-21 "$testcvs -q update" \
'A file1
M file2
R file3
C file4'

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
	  dotest join-23 "$testcvs -q update -jbranch ." \
"U file1
RCS file: $CVSROOT_DIRNAME/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
Merging differences between 1\.1 and 1\.1\.2\.2 into file2
$PROG update: scheduling file3 for removal
M file4
$PROG update: file file4 has been removed in revision branch, but the destination is incompatibly modified
C file4
$PROG update: file file9 does not exist, but is present in revision branch"

	  # Verify that the right changes have been scheduled.
	  # The M file2 line is a bug; see above join-20
	  dotest_fail join-24 "$testcvs -q update" \
'A file1
M file2
R file3
C file4'

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
	  dotest join-27 "$testcvs -q update -r br2" \
'[UP] file2
[UP] file3
[UP] file4
[UP] file7'
	  # The handling of file8 and file9 here look fishy to me.  I don't
	  # see why it should be different from the case where we merge to
	  # the trunk (e.g. join-23).
	  dotest join-28 "$testcvs -q update -j branch" \
"U file1
RCS file: $CVSROOT_DIRNAME/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
Merging differences between 1\.1 and 1\.1\.2\.2 into file2
$PROG update: scheduling file3 for removal
$PROG update: file file4 has been removed in revision branch, but the destination is incompatibly modified
C file4
U file8
U file9"
	  # Verify that the right changes have been scheduled.
	  dotest_fail join-29 "$testcvs -q update" \
"A file1
M file2
R file3
C file4
A file8
A file9"

	  # Checkout the mainline again to try updating and merging between two
	  # branches in the same step
	  # this seems a likely scenario - the user finishes up on branch and
	  # updates to br2 and merges in the same step - and there was a bug
	  # once that if the file was removed in the update then it wouldn't be
	  # readded in the merge
	  cd ..
	  rm -rf first-dir
	  dotest join-twobranch-1 "${testcvs} -q co -rbranch first-dir" \
'U first-dir/file1
U first-dir/file2
U first-dir/file8
U first-dir/file9'
	  cd first-dir
	  dotest join-twobranch-2 "$testcvs -q update -rbr2 -jbranch" \
"$PROG update: file1 is no longer in the repository
U file1
U file2
RCS file: $CVSROOT_DIRNAME/first-dir/file2,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.2
Merging differences between 1\.1 and 1\.1\.2\.2 into file2
U file3
$PROG update: scheduling file3 for removal
U file4
$PROG update: file file4 has been removed in revision branch, but the destination is incompatibly modified
C file4
U file7
$PROG update: file8 is no longer in the repository
U file8
$PROG update: file9 is no longer in the repository
U file9"
	  # Verify that the right changes have been scheduled.
	  dotest_fail join-twobranch-3 "$testcvs -q update" \
"A file1
M file2
R file3
C file4
A file8
A file9"

	  # Checkout the mainline again to try merging from the trunk
	  # to a branch.
	  cd ..
	  rm -r first-dir
	  dotest join-30 "${testcvs} -q co first-dir" \
'U first-dir/file2
U first-dir/file3
U first-dir/file4
U first-dir/file7'
	  cd first-dir

	  # Tag the current revisions on the trunk.
	  dotest join-31 "${testcvs} -q tag T3 ." \
'T file2
T file3
T file4
T file7'

	  # Modify file7.
	  echo 'second revision of file7' > file7
	  dotest join-32 "${testcvs} -q ci -mx ." \
"Checking in file7;
${CVSROOT_DIRNAME}/first-dir/file7,v  <--  file7
new revision: 1\.2; previous revision: 1\.1
done"

	  # And Tag again.
	  dotest join-33 "${testcvs} -q tag T4 ." \
'T file2
T file3
T file4
T file7'

	  # Now update branch to T3.
	  cd ../../2/first-dir
	  dotest join-34 "${testcvs} -q up -jT3" \
"${PROG} update: file file4 does not exist, but is present in revision T3
U file7"

	  # Verify that the right changes have been scheduled.
	  dotest join-35 "${testcvs} -q update" \
'A file7'

	  # Now update to T4.
	  # This is probably a bug, although in this particular case it just
	  # happens to do the right thing; see above join-20.
	  dotest join-36 "${testcvs} -q up -j T3 -j T4" \
"A file7
RCS file: ${CVSROOT_DIRNAME}/first-dir/file7,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into file7"

	  # Verify that the right changes have been scheduled.
	  dotest join-37 "${testcvs} -q update" \
'A file7'

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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir
	  echo 'initial contents of file1' >file1
	  dotest join2-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest join2-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest join2-5 "${testcvs} -q tag -b br1" "T file1"
	  dotest join2-6 "$testcvs -q update -r br1" '[UP] file1'
	  echo 'modify on branch' >>file1
	  touch bradd
	  dotest join2-6a "${testcvs} add bradd" \
"${PROG} add: scheduling file .bradd. for addition on branch .br1.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest join2-7 "${testcvs} -q ci -m modify" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/bradd,v
done
Checking in bradd;
${CVSROOT_DIRNAME}/first-dir/Attic/bradd,v  <--  bradd
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Here is the unusual/pathological part.  We switch back to
	  # the trunk *for file1 only*, not for the whole directory.
	  dotest join2-8 "${testcvs} -q update -A file1" '[UP] file1'
	  dotest join2-9 "${testcvs} -q status file1" \
"===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest join2-10 "cat CVS/Tag" "Tbr1"

	  dotest join2-11 "${testcvs} -q update -j br1 file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest join2-14 "cat CVS/Tag" "Tbr1"
	  # And the checkin should go to the trunk
	  dotest join2-15 "${testcvs} -q ci -m modify file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
"${PROG} update: bradd is no longer in the repository
[UP] file1"
:	  dotest join2-17 "${testcvs} -q update -A bradd" \
"${PROG} update: warning: bradd is not (any longer) pertinent"
	  dotest join2-18 "${testcvs} -q update -j br1 bradd" "U bradd"
	  dotest join2-19 "${testcvs} -q status bradd" \
"===================================================================
File: bradd            	Status: Locally Added

   Working revision:	New file!
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/Attic/bradd,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest join2-20 "${testcvs} -q ci -m modify bradd" \
"Checking in bradd;
${CVSROOT_DIRNAME}/first-dir/bradd,v  <--  bradd
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo 'initial contents of file1' >file1
	  dotest join3-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest join3-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest join3-5 "${testcvs} -q tag -b br1" "T file1"
	  dotest join3-6 "$testcvs -q update -r br1" '[UP] file1'
	  echo 'br1:line1' >>file1
	  dotest join3-7 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Now back to the trunk for:
	  # another revision and another branch for file1.
	  # add file2, which will exist on trunk and br2 but not br1.
	  dotest join3-8 "${testcvs} -q update -A" "[UP] file1"
	  echo 'trunk:line1' > file2
	  dotest join3-8a "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  echo 'trunk:line1' >>file1
	  dotest join3-9 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  dotest join3-10 "${testcvs} -q tag -b br2" "T file1
T file2"

	  # Before we actually have any revision on br2, let's try a join
	  dotest join3-11 "${testcvs} -q update -r br1" "[UP] file1
${PROG} update: file2 is no longer in the repository"
	  dotest join3-12 "${testcvs} -q update -j br2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
"${PROG} update: warning: file1 was lost
U file1" "U file1"
	  echo 'br2:line1' >>file1
	  dotest join3-15 "${testcvs} -q ci -m modify file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"

	  # OK, now we can join br2 to br1
	  dotest join3-16 "${testcvs} -q update -r br1 file1" "[UP] file1"
	  # It may seem odd, to merge a higher branch into a lower
	  # branch, but in fact CVS defines the ancestor as 1.1
	  # and so it merges both the 1.1->1.2 and 1.2->1.2.2.1 changes.
	  # This seems like a reasonably plausible behavior.
	  dotest join3-17 "${testcvs} -q update -j br2 file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	join4)
	  # Like join, but with local (uncommitted) modifications.

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest join4-1 "${testcvs} -q co first-dir" ''

	  cd first-dir

	  # Add two files.
	  echo 'first revision of file3' > file3
	  echo 'first revision of file4' > file4
	  echo 'first revision of file6' > file6
	  echo 'first revision of file8' > file8
	  echo 'first revision of file9' > file9
	  dotest join4-2 "${testcvs} add file3 file4 file6 file8 file9" \
"${PROG}"' add: scheduling file `file3'\'' for addition
'"${PROG}"' add: scheduling file `file4'\'' for addition
'"${PROG}"' add: scheduling file `file6'\'' for addition
'"${PROG}"' add: scheduling file `file8'\'' for addition
'"${PROG}"' add: scheduling file `file9'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'

	  dotest join4-3 "${testcvs} -q commit -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file4,v
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file6,v
done
Checking in file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file8,v
done
Checking in file8;
${CVSROOT_DIRNAME}/first-dir/file8,v  <--  file8
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file9,v
done
Checking in file9;
${CVSROOT_DIRNAME}/first-dir/file9,v  <--  file9
initial revision: 1\.1
done"

	  # Make a branch.
	  dotest join4-4 "${testcvs} -q tag -b branch ." \
'T file3
T file4
T file6
T file8
T file9'

	  # Add file10
	  echo 'first revision of file10' > file10
	  dotest join4-7a "${testcvs} add file10" \
"${PROG}"' add: scheduling file `file10'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest join4-7b "${testcvs} -q ci -mx ." \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file10,v
done
Checking in file10;
${CVSROOT_DIRNAME}/first-dir/file10,v  <--  file10
initial revision: 1\.1
done"

	  # Add file2 and file7, modify file4, and remove
	  # file6, file8, file9, and file10.
	  echo 'first revision of file2' > file2
	  echo 'second revision of file4' > file4
	  echo 'first revision of file7' > file7
	  rm file6 file8 file9 file10
	  dotest join4-5 "${testcvs} add file2 file7" \
"${PROG}"' add: scheduling file `file2'\'' for addition
'"${PROG}"' add: scheduling file `file7'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'
	  dotest join4-6 "${testcvs} rm file6 file8 file9 file10" \
"${PROG}"' remove: scheduling `file6'\'' for removal
'"${PROG}"' remove: scheduling `file8'\'' for removal
'"${PROG}"' remove: scheduling `file9'\'' for removal
'"${PROG}"' remove: scheduling `file10'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove these files permanently'

	  # Check out the branch.
	  cd ../..
	  mkdir 2
	  cd 2
	  dotest join4-8 "${testcvs} -q co -r branch first-dir" \
'U first-dir/file3
U first-dir/file4
U first-dir/file6
U first-dir/file8
U first-dir/file9'

	  cd first-dir

	  # Modify the files on the branch, so that T1 is not an
	  # ancestor of the main line, and add file5
	  echo 'first branch revision of file3' > file3
	  echo 'first branch revision of file4' > file4
	  echo 'first branch revision of file5' > file5
	  echo 'first branch revision of file6' > file6
	  echo 'first branch revision of file9' > file9
	  dotest join4-9 "${testcvs} add file5" \
"${PROG}"' add: scheduling file `file5'\'' for addition on branch `branch'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest join4-10 "${testcvs} -q ci -mx ." \
"Checking in file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file5,v
done
Checking in file5;
${CVSROOT_DIRNAME}/first-dir/Attic/file5,v  <--  file5
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in file9;
${CVSROOT_DIRNAME}/first-dir/file9,v  <--  file9
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Tag the current revisions on the branch.
	  dotest join4-11 "${testcvs} -q tag T1 ." \
'T file3
T file4
T file5
T file6
T file8
T file9'

	  # Add file1 and file2, modify file9, and remove the other files.
	  echo 'first branch revision of file1' > file1
	  echo 'first branch revision of file2' > file2
	  echo 'second branch revision of file9' > file9
	  rm file3 file4 file5 file6
	  dotest join4-12 "${testcvs} add file1 file2" \
"${PROG}"' add: scheduling file `file1'\'' for addition on branch `branch'\''
'"${PROG}"' add: scheduling file `file2'\'' for addition on branch `branch'\''
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'
	  dotest join4-13 "${testcvs} rm file3 file4 file5 file6" \
"${PROG}"' remove: scheduling `file3'\'' for removal
'"${PROG}"' remove: scheduling `file4'\'' for removal
'"${PROG}"' remove: scheduling `file5'\'' for removal
'"${PROG}"' remove: scheduling `file6'\'' for removal
'"${PROG}"' remove: use .'"${PROG}"' commit. to remove these files permanently'
	  dotest join4-14 "${testcvs} -q ci -mx ." \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/Attic/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/Attic/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Removing file3;
${CVSROOT_DIRNAME}/first-dir/file3,v  <--  file3
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file4;
${CVSROOT_DIRNAME}/first-dir/file4,v  <--  file4
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file5;
${CVSROOT_DIRNAME}/first-dir/Attic/file5,v  <--  file5
new revision: delete; previous revision: 1\.1\.2\.1
done
Removing file6;
${CVSROOT_DIRNAME}/first-dir/file6,v  <--  file6
new revision: delete; previous revision: 1\.1\.2\.1
done
Checking in file9;
${CVSROOT_DIRNAME}/first-dir/file9,v  <--  file9
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"

	  # Tag the current revisions on the branch.
	  dotest join4-15 "${testcvs} -q tag T2 ." \
'T file1
T file2
T file8
T file9'

	  # Modify file4 locally, and do an update with a merge.
	  cd ../../1/first-dir
	  echo 'third revision of file4' > file4
	  dotest join4-18 "$testcvs -q update -jT1 -jT2 ." \
"U file1
R file10
A file2
$PROG update: file file2 exists, but has been added in revision T2
$PROG update: scheduling file3 for removal
M file4
$PROG update: file file4 has been removed in revision T2, but the destination is incompatibly modified
C file4
R file6
A file7
R file8
R file9
$PROG update: file file9 does not exist, but is present in revision T2"

	  # Verify that the right changes have been scheduled.
	  dotest_fail join4-19 "${testcvs} -q update" \
'A file1
R file10
A file2
R file3
C file4
R file6
A file7
R file8
R file9'

	  cd ../..

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	join5)
	  # This test verifies that CVS can handle filenames starting with a
	  # dash (`-') properly.  What used to happen was that CVS handled it
	  # just fine, until it went to pass them as arguments to the diff
	  # library, at which point it neglected to pass `--' before the file
	  # list, causing the diff library to attempt to interpret the file
	  # name as an argument.
	  mkdir join5; cd join5
	  mkdir 1; cd 1
	  dotest join5-init-1 "${testcvs} -Q co -l ."
	  mkdir join5
	  dotest join5-init-2 "${testcvs} -Q add join5"
	  cd join5
	  echo "there once was a file from harrisburg" >-file
	  echo "who's existance it seems was quiteabsurd" >>-file
	  dotest join5-init-3 "${testcvs} -Q add -- -file"
	  dotest join5-init-4 "${testcvs} -q ci -minitial" \
"RCS file: ${CVSROOT_DIRNAME}/join5/-file,v
done
Checking in -file;
${CVSROOT_DIRNAME}/join5/-file,v  <--  -file
initial revision: 1\.1
done"
	  cd ../..

	  mkdir 2; cd 2
	  dotest join5-init-5 "${testcvs} -Q co join5"
	  cd join5
	  echo "it tested for free" >>-file
	  echo "when paid it should be" >>-file
	  dotest join5-init-4 "${testcvs} -q ci -msecond" \
"Checking in -file;
${CVSROOT_DIRNAME}/join5/-file,v  <--  -file
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../..

	  cd 1/join5
	  echo "but maybe it could charge bytheword" >>-file
	  # This is the test that used to spew complaints from diff3:
	  dotest join5 "${testcvs} up" \
"${PROG} update: Updating \.
RCS file: ${CVSROOT_DIRNAME}/join5/-file,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into -file
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in -file
C -file"
	  cd ../..

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  rm -r join5
	  rm -rf ${CVSROOT_DIRNAME}/join5
	  ;;

        join6)
	  mkdir join6; cd join6
          mkdir 1; cd 1
	  dotest join6-init-1 "${testcvs} -Q co -l ."
	  mkdir join6
	  dotest join6-init-2 "${testcvs} -Q add join6"
	  cd join6
          echo aaa >temp.txt
	  echo bbb >>temp.txt
	  echo ccc >>temp.txt
	  dotest join6-1 "${testcvs} -Q add temp.txt"
	  dotest join6-2 "${testcvs} -q commit -minitial temp.txt" \
"RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
done
Checking in temp\.txt;
${CVSROOT_DIRNAME}/join6/temp.txt,v  <--  temp\.txt
initial revision: 1\.1
done"
	  cp temp.txt temp2.txt
	  echo ddd >>temp.txt
	  dotest join6-3 "${testcvs} -q commit -madd temp.txt" \
"Checking in temp\.txt;
${CVSROOT_DIRNAME}/join6/temp.txt,v  <--  temp\.txt
new revision: 1\.2; previous revision: 1\.1
done"

	  # The case where the merge target is up-to-date and its base revision
	  # matches the second argument to -j: CVS doesn't bother attempting
	  # the merge since it already knows that the target contains the
	  # change.
	  dotest join6-3.3 "${testcvs} update -j1.1 -j1.2 temp.txt" \
"temp\.txt already contains the differences between 1\.1 and 1\.2"
	  dotest join6-3.4 "${testcvs} diff temp.txt" ""

	  # The case where the merge target is modified but already contains
	  # the change.
	  echo bbb >temp.txt
	  echo ccc >>temp.txt
	  echo ddd >>temp.txt
	  dotest join6-3.5 "${testcvs} update -j1.1 -j1.2 temp.txt" \
"M temp\.txt
RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into temp\.txt
temp\.txt already contains the differences between 1\.1 and 1\.2"
	  dotest_fail join6-3.6 "${testcvs} diff temp.txt" \
"Index: temp\.txt
===================================================================
RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
retrieving revision 1\.2
diff -r1\.2 temp.txt
1d0
< aaa"

	  cp temp2.txt temp.txt
	  dotest_fail join6-4 "${testcvs} diff temp.txt" \
"Index: temp.txt
===================================================================
RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
retrieving revision 1\.2
diff -r1\.2 temp\.txt
4d3
< ddd"

	  dotest join6-5 "${testcvs} update -j1.1 -j1.2 temp.txt" \
"M temp\.txt
RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into temp\.txt"
	  dotest join6-6 "${testcvs} diff temp.txt" ""
	  mv temp.txt temp3.txt
	  dotest join6-7 "sed 's/ddd/dddd/' < temp3.txt > temp.txt" ""
	  dotest join6-8 "${testcvs} update -j1.1 -j1.2 temp.txt" \
"M temp\.txt
RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into temp\.txt
rcsmerge: warning: conflicts during merge"
	  dotest_fail join6-9 "${testcvs} diff temp.txt" \
"Index: temp\.txt
===================================================================
RCS file: ${CVSROOT_DIRNAME}/join6/temp.txt,v
retrieving revision 1\.2
diff -r1\.2 temp\.txt
3a4,6
> <<<<<<< temp\.txt
> dddd
> =======
4a8
> >>>>>>> 1\.2"
	  cp temp2.txt temp.txt
	  dotest join6-10 "${testcvs} -q ci -m del temp.txt" \
"Checking in temp\.txt;
${CVSROOT_DIRNAME}/join6/temp.txt,v  <--  temp\.txt
new revision: 1\.3; previous revision: 1\.2
done"
          cp temp3.txt temp.txt
	  dotest_fail join6-11 "${testcvs} diff temp.txt" \
"Index: temp\.txt
===================================================================
RCS file: ${CVSROOT_DIRNAME}/join6/temp.txt,v
retrieving revision 1\.3
diff -r1\.3 temp\.txt
3a4
> ddd"
	  dotest join6-12 "${testcvs} update -j1.2 -j1.3 temp.txt" \
"M temp\.txt
RCS file: ${CVSROOT_DIRNAME}/join6/temp\.txt,v
retrieving revision 1\.2
retrieving revision 1\.3
Merging differences between 1\.2 and 1\.3 into temp\.txt"
	  dotest join6-13 "${testcvs} diff temp.txt" ""

	  # The case where the merge target wasn't created until after the
	  # first tag was applied
	  rm temp2.txt temp3.txt
	  dotest join6-20 "${testcvs} -q tag -r1.1 t1" \
"T temp.txt"
	  echo xxx >temp2.txt
	  dotest join6-21 "${testcvs} -Q add temp2.txt"
	  dotest join6-22 "${testcvs} -q ci -m." \
"RCS file: ${CVSROOT_DIRNAME}/join6/temp2.txt,v
done
Checking in temp2\.txt;
${CVSROOT_DIRNAME}/join6/temp2\.txt,v  <--  temp2\.txt
initial revision: 1\.1
done"
	  dotest join6-23 "${testcvs} -q tag t2" \
"T temp.txt
T temp2.txt"
	  echo xxx >>temp.txt
	  dotest join6-24 "${testcvs} -q ci -m." \
"Checking in temp\.txt;
${CVSROOT_DIRNAME}/join6/temp.txt,v  <--  temp\.txt
new revision: 1\.4; previous revision: 1\.3
done"
	  dotest join6-25 "${testcvs} -q up -jt1 -jt2" \
"RCS file: ${CVSROOT_DIRNAME}/join6/temp.txt,v
retrieving revision 1\.1
retrieving revision 1\.3
Merging differences between 1\.1 and 1\.3 into temp.txt
temp.txt already contains the differences between 1\.1 and 1\.3
temp2.txt already contains the differences between creation and 1\.1"

	  # Now for my next trick: delete the file, recreate it, and
	  # try to merge
	  dotest join6-30 "${testcvs} -q rm -f temp2.txt" \
"${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest join6-31 "${testcvs} -q ci -m. temp2.txt" \
"Removing temp2\.txt;
${CVSROOT_DIRNAME}/join6/temp2\.txt,v  <--  temp2\.txt
new revision: delete; previous revision: 1\.1
done"
	  echo new >temp2.txt
	  # FIXCVS: Local and remote really shouldn't be different and there
	  # really shouldn't be two different status lines for temp2.txt
	  if $remote; then
	    dotest_fail join6-32 "${testcvs} -q up -jt1 -jt2" \
"? temp2\.txt
RCS file: ${CVSROOT_DIRNAME}/join6/temp.txt,v
retrieving revision 1\.1
retrieving revision 1\.3
Merging differences between 1\.1 and 1\.3 into temp.txt
temp.txt already contains the differences between 1\.1 and 1\.3
${PROG} update: move away \./temp2\.txt; it is in the way
C temp2\.txt"
	  else
	    dotest join6-32 "${testcvs} -q up -jt1 -jt2" \
"RCS file: ${CVSROOT_DIRNAME}/join6/temp.txt,v
retrieving revision 1\.1
retrieving revision 1\.3
Merging differences between 1\.1 and 1\.3 into temp.txt
temp.txt already contains the differences between 1\.1 and 1\.3
${PROG} update: use .${PROG} add. to create an entry for temp2\.txt
U temp2\.txt
? temp2\.txt"
	  fi

	  cd ../../..

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
            exit 0
	  fi

	  rm -r join6
	  rm -rf ${CVSROOT_DIRNAME}/join6
	  ;;

	join7)
	  # This test deals with joins that happen with the -n switch
	  mkdir join7; cd join7
	  mkdir impdir; cd impdir
          echo aaa >temp.txt
	  echo bbb >>temp.txt
	  echo ccc >>temp.txt
	  dotest join7-1 \
"${testcvs} -Q import -minitial join7 vendor vers-1" \
""
	  cd ..
	  dotest join7-2 "${testcvs} -Q co join7" ""
	  cd join7
	  echo ddd >> temp.txt
	  dotest join7-3 "${testcvs} -Q ci -madded-line temp.txt" \
"Checking in temp.txt;
$CVSROOT_DIRNAME/join7/temp.txt,v  <--  temp.txt
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../impdir
	  echo aaaa >temp.txt
	  echo bbbb >>temp.txt
	  echo ccc >>temp.txt
	  echo eee >>temp.txt
	  dotest join7-4 \
"${testcvs} -Q import -minitial join7 vendor vers-2" \
""
	  cd ../join7
	  dotest join7-5 \
"${testcvs} -n update -jvers-1 -jvers-2 temp.txt" \
"RCS file: $CVSROOT_DIRNAME/join7/temp.txt,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.2
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.2 into temp.txt
rcsmerge: warning: conflicts during merge"
	  touch temp.txt
	  dotest join7-6 "${testcvs} -n update -jvers-1 -jvers-2 temp.txt" \
"RCS file: $CVSROOT_DIRNAME/join7/temp.txt,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.2
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.2 into temp.txt
rcsmerge: warning: conflicts during merge" \
"RCS file: $CVSROOT_DIRNAME/join7/temp.txt,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.2
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.2 into temp.txt
rcsmerge: warning: conflicts during merge"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
            exit 0
	  fi

	  cd ../..
	  rm -r join7
	  rm -rf $CVSROOT_DIRNAME/join7
	  ;;



        join8)
	  # In this test case, we have 2 projects, one called "pvcs" and one
	  # called "project".  The "pvcs" project has modified the file, while
	  # the "project" project has caused a deletion.  When "project" is
	  # merged into "pvcs", we expect CVS to detect a conflict.
          mkdir join8; cd join8
          mkdir combine
          mkdir base
          mkdir pvcs
          mkdir project
       
          echo "aaa" >base/file.txt
          echo "bbb" >pvcs/file.txt
          echo "ccc" >project/xxx.txt
       
          cd base
          dotest join8-1 \
"$testcvs import -b 1.1.101 -ko -m 'base import' join8 base base-1" \
"N join8/file\.txt

No conflicts created by this import"
       
          cd ../pvcs
          dotest join8-2 \
"$testcvs import -b 1.1.201 -ko -m 'pvcs import' join8 pvcs pvcs-1" \
"C join8/file\.txt

1 conflicts created by this import.
Use the following command to help the merge:

	$PROG checkout -j<prev_rel_tag> -jpvcs-1 join8"

          cd ../project
          dotest join8-3 \
"$testcvs import -b 1.1.301 -ko -m 'project import' join8 project project-1" \
"N join8/xxx\.txt

No conflicts created by this import"

          cd ..
          dotest join8-4 \
"$testcvs checkout -r pvcs-1 -j base-1 -j project-1 -d combine join8" \
"$PROG checkout: Updating combine
U combine/file\.txt
$PROG checkout: file combine/file\.txt has been removed in revision project-1, but the destination is incompatibly modified
C combine/file.txt
U combine/xxx\.txt"

          dotest join8-5 \
"$testcvs -Q up -pr base-1 combine/file.txt >combine/file.txt"

          dotest join8-6 \
"$testcvs up -j base-1 -j project-1 combine" \
"$PROG update: Updating combine
M combine/file\.txt
$PROG update: scheduling combine/file\.txt for removal
A combine/xxx\.txt
$PROG update: file combine/xxx\.txt exists, but has been added in revision project-1"
          cd ..
       
          if $keep; then
            echo Keeping $TESTDIR and exiting due to --keep
                   exit 0
          fi
       
          rm -r join8
          rm -rf $CVSROOT_DIRNAME/join8
          ;;



        join9)
	  # In this test case, we have 2 projects, one called "pvcs" and one
	  # called "project".  The "pvcs" project has not modified the file,
	  # while the "project" project has caused a deletion.  When "project"
	  # is merged into "pvcs", we expect CVS to remove the file without
	  # fuss, as there is no conflict.
          mkdir join9; cd join9
          mkdir combine
          mkdir base
          mkdir pvcs
          mkdir project
       
          echo "aaa" >base/file.txt
          echo "aaa" >pvcs/file.txt
          echo "ccc" >project/xxx.txt
       
          cd base
          dotest join9-1 \
"$testcvs import -b 1.1.101 -ko -m 'base import' join9 base base-1" \
"N join9/file\.txt

No conflicts created by this import"

          cd ../pvcs
          dotest join9-2 \
"$testcvs import -b 1.1.201 -ko -m 'pvcs import' join9 pvcs pvcs-1" \
"C join9/file\.txt

1 conflicts created by this import.
Use the following command to help the merge:

	$PROG checkout -j<prev_rel_tag> -jpvcs-1 join9"

          cd ../project
          dotest join9-3 \
"$testcvs import -b 1.1.301 -ko -m 'project import' join9 project project-1" \
"N join9/xxx\.txt

No conflicts created by this import"

          cd ..
          dotest join9-4 \
"$testcvs checkout -r pvcs-1 -j base-1 -j project-1 -d combine join9" \
"$PROG checkout: Updating combine
U combine/file\.txt
$PROG checkout: scheduling combine/file\.txt for removal
U combine/xxx\.txt"

          cd ..

          if $keep; then
            echo Keeping $TESTDIR and exiting due to --keep
                   exit 0
          fi

          rm -r join9
          rm -rf $CVSROOT_DIRNAME/join9
         ;;



	join-readonly-conflict)
	  # Previously, only tests 1 & 11 were being tested.  I added the
	  # intermediate dotest's to try and diagnose a different failure
	  #
	  # Demonstrate that cvs-1.9.29 can fail on 2nd and subsequent
	  # conflict-evoking join attempts.
	  # Even with that version of CVS, This test failed only in
	  # client-server mode, and would have been noticed in normal
	  # operation only for files that were read-only (either due to
	  # use of cvs' global -r option, setting the CVSREAD envvar,
	  # or use of watch lists).
	  mkdir join-readonly-conflict; cd join-readonly-conflict
	  dotest join-readonly-conflict-1 "$testcvs -q co -l ." ''
	  module=join-readonly-conflict
	  mkdir $module
	  $testcvs -q add $module >>$LOGFILE 2>&1
	  cd $module

	  file=m
	  echo trunk > $file
	  dotest join-readonly-conflict-2 "$testcvs -Q add $file" ''

	  dotest join-readonly-conflict-3 "$testcvs -q ci -m . $file" \
"RCS file: $CVSROOT_DIRNAME/$module/$file,v
done
Checking in $file;
$CVSROOT_DIRNAME/$module/$file,v  <--  $file
initial revision: 1\.1
done"

	  dotest join-readonly-conflict-4 "$testcvs tag -b B $file" "T $file"
	  dotest join-readonly-conflict-5 "$testcvs -q update -rB $file" \
"[UP] $file"
	  echo branch B > $file
	  dotest join-readonly-conflict-6 "$testcvs -q ci -m . $file" \
"Checking in $file;
$CVSROOT_DIRNAME/$module/$file,v  <--  $file
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  rm $file
	  dotest join-readonly-conflict-7 "$testcvs -Q update -A $file" ''
	  # Make sure $file is read-only.  This can happen more realistically
	  # via patch -- which could be used to apply a delta, yet would
	  # preserve a file's read-only permissions.
	  echo conflict > $file; chmod u-w $file
	  dotest join-readonly-conflict-8 "$testcvs update -r B $file" \
"RCS file: $CVSROOT_DIRNAME/$module/$file,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into $file
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in $file
C $file"

	  # restore to the trunk
	  rm -f $file
	  dotest join-readonly-conflict-9 "$testcvs -Q update -A $file" ''

	  # This one would fail because cvs couldn't open the existing
	  # (and read-only) .# file for writing.
	  echo conflict > $file

	  # verify that the backup file is not writable
	  if test -w ".#$file.1.1"; then
	    fail "join-readonly-conflict-10 : .#$file.1.1 is writable"
	  else
	    pass "join-readonly-conflict-10"
	  fi
	  dotest join-readonly-conflict-11 "$testcvs update -r B $file" \
"RCS file: $CVSROOT_DIRNAME/$module/$file,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into $file
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in $file
C m"

	  cd ../..
	  if $keep; then :; else
	    rm -rf join-readonly-conflict
	    rm -rf $CVSROOT_DIRNAME/$module
	  fi
	  ;;

	join-admin)
	  mkdir 1; cd 1
	  dotest join-admin-1 "$testcvs -q co -l ." ''
	  module=x
	  mkdir $module
	  $testcvs -q add $module >>$LOGFILE 2>&1
	  cd $module

	  # Create a file so applying the first tag works.
	  echo foo > a
	  $testcvs -Q add a > /dev/null 2>&1
	  $testcvs -Q ci -m. a > /dev/null 2>&1

	  $testcvs -Q tag -b B
	  $testcvs -Q tag -b M1
	  echo '$''Id$' > b
	  $testcvs -Q add b > /dev/null 2>&1
	  $testcvs -Q ci -m. b > /dev/null 2>&1
	  $testcvs -Q tag -b M2

	  $testcvs -Q update -r B
	  $testcvs -Q update -kk -jM1 -jM2
	  $testcvs -Q ci -m. b >/dev/null 2>&1

	  $testcvs -Q update -A

	  # Verify that the -kk flag from the update did not
	  # propagate to the repository.
	  dotest join-admin-1 "$testcvs status b" \
"===================================================================
File: b                	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/x/b,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	join-admin-2)
	  # Show that when a merge (via update -kk -jtag1 -jtag2) first
	  # removes a file, then modifies another containing an $Id...$ line,
	  # the resulting file contains the unexpanded `$Id.$' string, as
	  # -kk requires.
	  mkdir 1; cd 1
	  dotest join-admin-2-1 "$testcvs -q co -l ." ''
	  module=x
	  mkdir $module
	  dotest join-admin-2-2 "$testcvs -q add $module" \
"Directory ${CVSROOT_DIRNAME}/x added to the repository"
	  cd $module

	  # Create a file so applying the first tag works.
	  echo '$''Id$' > e0
	  cp e0 e
	  dotest join-admin-2-3 "$testcvs -Q add e" ''
	  dotest join-admin-2-4 "$testcvs -Q ci -m. e" \
"RCS file: ${CVSROOT_DIRNAME}/x/e,v
done
Checking in e;
${CVSROOT_DIRNAME}/x/e,v  <--  e
initial revision: 1\.1
done"

	  dotest join-admin-2-5 "$testcvs -Q tag -b T" '' "${QUESTION} e0"
	  dotest join-admin-2-6 "$testcvs -Q update -r T" '' "${QUESTION} e0"
	  cp e0 e
	  dotest join-admin-2-7 "$testcvs -Q ci -m. e" \
"Checking in e;
${CVSROOT_DIRNAME}/x/e,v  <--  e
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  dotest join-admin-2-8 "$testcvs -Q update -A" '' "${QUESTION} e0"
	  dotest join-admin-2-9 "$testcvs -Q tag -b M1" '' "${QUESTION} e0"

	  echo '$''Id$' > b
	  dotest join-admin-2-10 "$testcvs -Q add b" ''
	  cp e0 e
	  dotest join-admin-2-11 "$testcvs -Q ci -m. b e" \
"RCS file: ${CVSROOT_DIRNAME}/x/b,v
done
Checking in b;
${CVSROOT_DIRNAME}/x/b,v  <--  b
initial revision: 1\.1
done
Checking in e;
${CVSROOT_DIRNAME}/x/e,v  <--  e
new revision: 1\.2; previous revision: 1\.1
done"

	  dotest join-admin-2-12 "$testcvs -Q tag -b M2" '' "${QUESTION} e0"

	  dotest join-admin-2-13 "$testcvs -Q update -r T" '' "${QUESTION} e0"
	  dotest join-admin-2-14 "$testcvs update -kk -jM1 -jM2" \
"${PROG} update: Updating .
U b
U e
RCS file: ${CVSROOT_DIRNAME}/x/e,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into e
e already contains the differences between 1\.1 and 1\.2
${QUESTION} e0" \
"${QUESTION} e0
${PROG} update: Updating .
U b
U e
RCS file: ${CVSROOT_DIRNAME}/x/e,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into e
e already contains the differences between 1\.1 and 1\.2"

	  # Verify that the $Id.$ string is not expanded.
	  dotest join-admin-2-15 "cat e" '$''Id$'

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	join-rm)
	  # This first half of this test checks that a single-argument merge
	  # from a branch is capable of removing files.
	  #
	  # The second half verifies that an update to another location with an
	  # uncommitted removal will transfer the destination branch of the
	  # removal.

	  module=join-rm
	  mkdir $module; cd $module

	  dotest join-rm-init-1 "$testcvs -q co -l ." ''
	  mkdir $module
	  dotest join-rm-init-2 "$testcvs -q add $module" \
"Directory $CVSROOT_DIRNAME/$module added to the repository"
	  cd $module

	  # add some files.
	  touch a b c d e f g
	  dotest join-rm-init-3 "$testcvs -Q add a b c d e f g"
	  dotest join-rm-init-4 "$testcvs -Q ci -m add-em" \
"RCS file: $CVSROOT_DIRNAME/join-rm/a,v
done
Checking in a;
$CVSROOT_DIRNAME/join-rm/a,v  <--  a
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/join-rm/b,v
done
Checking in b;
$CVSROOT_DIRNAME/join-rm/b,v  <--  b
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/join-rm/c,v
done
Checking in c;
$CVSROOT_DIRNAME/join-rm/c,v  <--  c
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/join-rm/d,v
done
Checking in d;
$CVSROOT_DIRNAME/join-rm/d,v  <--  d
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/join-rm/e,v
done
Checking in e;
$CVSROOT_DIRNAME/join-rm/e,v  <--  e
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/join-rm/f,v
done
Checking in f;
$CVSROOT_DIRNAME/join-rm/f,v  <--  f
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/join-rm/g,v
done
Checking in g;
$CVSROOT_DIRNAME/join-rm/g,v  <--  g
initial revision: 1\.1
done"
	  
	  # create the branch and update to it
	  dotest join-rm-init-5 "$testcvs -Q tag -b br"
	  dotest join-rm-init-6 "$testcvs -Q up -rbr"

	  # remove a few files from the branch
	  dotest join-rm-init-7 "$testcvs -Q rm -f b d g"
	  dotest join-rm-init-8 "$testcvs -Q ci -mrm" \
"Removing b;
$CVSROOT_DIRNAME/join-rm/b,v  <--  b
new revision: delete; previous revision: 1\.1
done
Removing d;
$CVSROOT_DIRNAME/join-rm/d,v  <--  d
new revision: delete; previous revision: 1\.1
done
Removing g;
$CVSROOT_DIRNAME/join-rm/g,v  <--  g
new revision: delete; previous revision: 1\.1
done"

	  # update to the trunk
	  dotest join-rm-init-9 "$testcvs -Q up -A"

	  # now for the test - try and merge the removals.
	  dotest join-rm-1 "$testcvs -q up -jbr" \
"$PROG update: scheduling b for removal
$PROG update: scheduling d for removal
$PROG update: scheduling g for removal"

	  # And make sure the merge took
	  dotest join-rm-2 "$testcvs -qn up" \
"R b
R d
R g"

	  dotest join-rm-3 "$testcvs -q ci -m 'save the merge'" \
"Removing b;
$CVSROOT_DIRNAME/join-rm/b,v  <--  b
new revision: delete; previous revision: 1\.1
done
Removing d;
$CVSROOT_DIRNAME/join-rm/d,v  <--  d
new revision: delete; previous revision: 1\.1
done
Removing g;
$CVSROOT_DIRNAME/join-rm/g,v  <--  g
new revision: delete; previous revision: 1\.1
done"

	  # and verify that it was the head revision which was removed.
	  dotest join-rm-4 "$testcvs -q log b"  "
RCS file: $CVSROOT_DIRNAME/join-rm/Attic/b,v
Working file: b
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	br: 1\.1\.0\.2
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: $username;  state: dead;  lines: ${PLUS}0 -0
save the merge
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
branches:  1.1.2;
add-em
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: dead;  lines: ${PLUS}0 -0
rm
============================================================================="

	  # go back to the branch to set up for the second set of tests
	  dotest join-rm-init-10 "$testcvs -Q up -rbr"
	  dotest join-rm-init-11 "$testcvs -Q rm -f a"
	  dotest join-rm-init-12 "$testcvs -Q ci -m rma" \
"Removing a;
$CVSROOT_DIRNAME/join-rm/a,v  <--  a
new revision: delete; previous revision: 1\.1
done"

	  # now the test: update to the trunk
	  #
	  # FIXCVS: This update should merge the removal to the trunk.  It does
	  # not.
	  dotest join-rm-5 "$testcvs -q up -A" \
'U a
U c
U e
U f'

	  # and verify that there is no sticky tag
	  dotest join-rm-6 "$testcvs status a" \
"===================================================================
File: a                	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/join-rm/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
            exit 0
	  fi

	  cd ../..
	  rm -rf $CVSROOT_DIRNAME/$module
	  rm -r $module
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
"${PROG} add: scheduling file .a. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest newb-123c "${testcvs} -q ci -m added" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a,v
done
Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
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
"${PROG} remove: scheduling .a. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest newb-123h "${testcvs} -q ci -m removed" \
"Removing a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
new revision: delete; previous revision: 1\.1
done"

	  # Check out the file on the branch.  This should report
	  # that the file is not pertinent, but it should not
	  # say anything else.
	  cd ..
	  rm -r first-dir
	  dotest newb-123i "${testcvs} -q co -r branch first-dir/a" \
"${PROG} checkout: warning: first-dir/a is not (any longer) pertinent"

	  # Update the other copy, and make sure that a is removed.
	  cd ../1/first-dir
	  # "Entry Invalid" is a rather strange output here.  Something like
	  # "Removed in Repository" would make more sense.
	  dotest newb-123j0 "${testcvs} status a" \
"${PROG} status: a is no longer in the repository
===================================================================
File: a                	Status: Entry Invalid

   Working revision:	1\.1.*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/a,v
   Sticky Tag:		branch (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest newb-123j "${testcvs} -q update" \
"${PROG} update: a is no longer in the repository"

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
"${PROG} add: scheduling file .a. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
		dotest conflicts-126 "${testcvs} -q ci -m added" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a,v
done
Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
initial revision: 1\.1
done"

		cd ../..
		mkdir 2
		cd 2

		dotest conflicts-126.5 "${testcvs} co -p first-dir" \
"${PROG} checkout: Updating first-dir
===================================================================
Checking out first-dir/a
RCS:  ${CVSROOT_DIRNAME}/first-dir/a,v
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
"Directory ${CVSROOT_DIRNAME}/first-dir/dir1 added to the repository"
		dotest conflicts-128 "${testcvs} -q ci -m changed" \
"Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done"
		cd ../..

		# Similar to conflicts-126.5, but now the file has nonempty
		# contents.
		mkdir 3
		cd 3
		dotest conflicts-128.5 "${testcvs} co -p -l first-dir" \
"${PROG} checkout: Updating first-dir
===================================================================
Checking out first-dir/a
RCS:  ${CVSROOT_DIRNAME}/first-dir/a,v
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
"${PROG}"' commit: Up-to-date check failed for `a'\''
'"${PROG}"' \[commit aborted\]: correct above errors first!'
		mkdir dir1
		mkdir sdir
		dotest conflicts-status-0 "${testcvs} status a" \
"===================================================================
File: a                	Status: Needs Merge

   Working revision:	1\.1.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest conflicts-129a "${testcvs} -nq update a" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in a
C a"
		dotest conflicts-130 "${testcvs} -q update" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in a
C a
${QUESTION} dir1
${QUESTION} sdir" \
"${QUESTION} dir1
${QUESTION} sdir
RCS file: ${CVSROOT_DIRNAME}/first-dir/a,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into a
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in a
C a"
		rmdir dir1 sdir

		dotest conflicts-status-1 "${testcvs} status a" \
"===================================================================
File: a                	Status: Unresolved Conflict

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest_fail conflicts-131 "${testcvs} -q ci -m try" \
"${PROG} commit: file .a. had a conflict and has not been modified
${PROG} \[commit aborted\]: correct above errors first!"

		# Try to check in the file with the conflict markers in it.
		# Make sure we detect any one of the three conflict markers
		mv a aa
		grep '^<<<<<<<' aa >a
		dotest conflicts-status-2 "${testcvs} -nq ci -m try a" \
"${PROG} commit: warning: file .a. seems to still contain conflict indicators"

		grep '^=======' aa >a
		dotest conflicts-status-3 "${testcvs} -nq ci -m try a" \
"${PROG} commit: warning: file .a. seems to still contain conflict indicators"

		grep '^>>>>>>>' aa >a
		dotest conflicts-status-4 "${testcvs} -qn ci -m try a" \
"${PROG} commit: warning: file .a. seems to still contain conflict indicators"

		mv aa a
		echo lame attempt at resolving it >>a
		dotest conflicts-status-5 "${testcvs} status a" \
"===================================================================
File: a                	Status: File had conflicts on merge

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest conflicts-132 "${testcvs} -q ci -m try" \
"${PROG} commit: warning: file .a. seems to still contain conflict indicators
Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
new revision: 1\.3; previous revision: 1\.2
done"

		# OK, the user saw the warning (good user), and now
		# resolves it for real.
		echo resolve conflict >a
		dotest conflicts-status-6 "${testcvs} status a" \
"===================================================================
File: a                	Status: Locally Modified

   Working revision:	1\.3.*
   Repository revision:	1\.3	${CVSROOT_DIRNAME}/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
		dotest conflicts-133 "${testcvs} -q ci -m resolved" \
"Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
new revision: 1\.4; previous revision: 1\.3
done"
		dotest conflicts-status-7 "${testcvs} status a" \
"===================================================================
File: a                	Status: Up-to-date

   Working revision:	1\.4.*
   Repository revision:	1\.4	${CVSROOT_DIRNAME}/first-dir/a,v
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
"${PROG} add: scheduling file .a. for addition
${PROG} add: scheduling file .abc. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest conflicts2-142a3 "${testcvs} -q ci -m added" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a,v
done
Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
done
Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
initial revision: 1\.1
done"

	  cd ../..
	  mkdir 2
	  cd 2

	  dotest conflicts2-142a4 "${testcvs} -q co first-dir" 'U first-dir/a
U first-dir/abc'
	  cd ..

	  # BEGIN TESTS USING THE FILE A
	  # FIXME: would be cleaner to separate them out into their own
	  # tests; conflicts2 is getting long.
	  # Now test that if one person modifies and commits a
	  # file and a second person removes it, it is a
	  # conflict
	  cd 1/first-dir
	  echo modify a >>a
	  dotest conflicts2-142b2 "${testcvs} -q ci -m modify-a" \
"Checking in a;
${CVSROOT_DIRNAME}/first-dir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../../2/first-dir
	  rm a
	  dotest conflicts2-142b3 "${testcvs} rm a" \
"${PROG} remove: scheduling .a. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest_fail conflicts2-142b4 "${testcvs} -q update" \
"${PROG} update: conflict: removed a was modified by second party
C a"
	  # Resolve the conflict by deciding not to remove the file
	  # after all.
	  dotest_sort conflicts2-142b5 "${testcvs} add a" "U a
${PROG} add: a, version 1\.1, resurrected"
	  dotest conflicts2-142b5b1 "$testcvs status a" \
"===================================================================
File: a                	Status: Needs Patch

   Working revision:	1\.1.*
   Repository revision:	1\.2	$CVSROOT_DIRNAME/first-dir/a,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest conflicts2-142b6 "${testcvs} -q update" 'U a'

	  # Now one level up.
	  cd ..
	  dotest conflicts2-142b7 "${testcvs} rm -f first-dir/a" \
"${PROG} remove: scheduling .first-dir/a. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"

	  if $remote; then
	    # Haven't investigated this one.
	    dotest_fail conflicts2-142b8r "$testcvs add first-dir/a" \
"${PROG} add: in directory \.:
${PROG} \[add aborted\]: there is no version here; do '${PROG} checkout' first"
	    cd first-dir
	  else
	    dotest conflicts2-142b8 "${testcvs} add first-dir/a" \
"U first-dir/a
${PROG} add: first-dir/a, version 1\.2, resurrected"
	    cd first-dir
	    # Now recover from the damage that the 142b8 test did.
	    dotest conflicts2-142b9 "${testcvs} rm -f a" \
"${PROG} remove: scheduling .a. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  fi

	  dotest_sort conflicts2-142b10 "${testcvs} add a" "U a
${PROG} add: a, version 1\.2, resurrected"
	  # As with conflicts2-142b6, check that things are normal again.
	  dotest conflicts2-142b11 "${testcvs} -q update" ''
	  cd ../..
	  # END TESTS USING THE FILE A

	  # Now test that if one person removes a file and
	  # commits it, and a second person removes it, is it
	  # not a conflict.
	  cd 1/first-dir
	  rm abc
	  dotest conflicts2-142c0 "${testcvs} rm abc" \
"${PROG} remove: scheduling .abc. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest conflicts2-142c1 "${testcvs} -q ci -m remove-abc" \
"Removing abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
new revision: delete; previous revision: 1\.1
done"
	  cd ../../2/first-dir
	  rm abc
	  dotest conflicts2-142c2 "${testcvs} rm abc" \
"${PROG} remove: scheduling .abc. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest conflicts2-142c3 "${testcvs} update" \
"${PROG} update: Updating \."
	  cd ../..

	  # conflicts2-142d*: test that if one party adds a file, and another
	  # party has a file of the same name, cvs notices
	  cd 1/first-dir
	  touch aa.c
	  echo 'contents unchanged' >same.c
	  dotest conflicts2-142d0 "${testcvs} add aa.c same.c" \
"${PROG} add: scheduling file .aa\.c. for addition
${PROG} add: scheduling file .same\.c. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest conflicts2-142d1 "${testcvs} -q ci -m added" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aa\.c,v
done
Checking in aa\.c;
${CVSROOT_DIRNAME}/first-dir/aa\.c,v  <--  aa\.c
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/same\.c,v
done
Checking in same\.c;
${CVSROOT_DIRNAME}/first-dir/same\.c,v  <--  same\.c
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
	  if $remote; then
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
"${PROG} status: move away aa\.c; it is in the way
===================================================================
File: aa\.c             	Status: Unresolved Conflict

   Working revision:	No entry for aa\.c
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/aa\.c,v"

	  # Could also be testing the case in which the cvs add happened
	  # before the commit by the other user.
	  # This message seems somewhat bogus.  I mean, parallel development
	  # means that we get to work in parallel if we choose, right?  And
	  # then at commit time it would be a conflict.
	  dotest_fail conflicts2-142d4 "${testcvs} -q add aa.c" \
"${PROG} add: aa.c added independently by second party"

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

	  rm -r 1 2; rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	conflicts3)
	  # More tests of conflicts and/or multiple working directories
	  # in general.

	  mkdir 1; cd 1
	  dotest conflicts3-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest conflicts3-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd ..
	  mkdir 2; cd 2
	  dotest conflicts3-3 "${testcvs} -q co -l first-dir" ''
	  cd ../1/first-dir
	  touch file1 file2
	  dotest conflicts3-4 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest conflicts3-5 "${testcvs} -q ci -m add-them" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
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
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: scheduling .file2. for removal
${PROG} remove: use .${PROG} commit. to remove these files permanently"
	  dotest conflicts3-11 "${testcvs} -q ci -m remove-them" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done
Removing file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: delete; previous revision: 1\.1
done"
	  cd ../../1/first-dir
	  dotest conflicts3-12 "${testcvs} -n -q update" \
"${PROG} update: file1 is no longer in the repository
${PROG} update: file2 is no longer in the repository"
	  dotest conflicts3-13 "${testcvs} -q update" \
"${PROG} update: file1 is no longer in the repository
${PROG} update: file2 is no longer in the repository"

	  # OK, now add a directory to both working directories
	  # and see that CVS doesn't lose its mind.
	  mkdir sdir
	  dotest conflicts3-14 "${testcvs} add sdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir added to the repository"
	  touch sdir/sfile
	  dotest conflicts3-14a "${testcvs} add sdir/sfile" \
"${PROG} add: scheduling file .sdir/sfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest conflicts3-14b "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir/sfile,v
done
Checking in sdir/sfile;
${CVSROOT_DIRNAME}/first-dir/sdir/sfile,v  <--  sfile
initial revision: 1\.1
done"

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
	  if $remote; then
	    dotest conflicts3-15 "${testcvs} -q update" \
"${QUESTION} sdir"
	  else
	    dotest conflicts3-15 "${testcvs} -q update" \
"${QUESTION} sdir
${PROG} update: ignoring sdir (CVS/Repository missing)"
	    touch sdir/CVS/Repository
	    dotest conflicts3-16 "${testcvs} -q update" \
"${QUESTION} sdir
${PROG} update: ignoring sdir (CVS/Entries missing)"
	    cd ..
	    dotest conflicts3-16a "${testcvs} -q update first-dir" \
"${QUESTION} first-dir/sdir
${PROG} update: ignoring first-dir/sdir (CVS/Entries missing)"
	    cd first-dir
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
	  cd ..
	  dotest conflicts3-20 "${testcvs} -q update first-dir" \
"${PROG} [a-z]*: ignoring first-dir/newdir (CVS/Entries missing)"
	  cd first-dir
	  rm -r newdir

	  # The previous tests have left CVS/Entries in something of a mess.
	  # While we "should" be able to deal with that (maybe), for now
	  # we just start over.
	  cd ..
	  rm -r first-dir
	  dotest conflicts3-20a "${testcvs} -q co -l first-dir" ''
	  cd first-dir

	  dotest conflicts3-21 "${testcvs} -q update -d sdir" "U sdir/sfile"
	  rm -r sdir/CVS
	  dotest conflicts3-22 "${testcvs} -q update" "${QUESTION} sdir"
	  if $remote; then
	    dotest_fail conflicts3-23 "${testcvs} -q update -PdA" \
"${QUESTION} sdir
${PROG} update: move away sdir/sfile; it is in the way
C sdir/sfile"
	  else
	    dotest conflicts3-23 "${testcvs} -q update -PdA" \
"${QUESTION} sdir"
	  fi

	  # Not that it should really affect much, but let's do the case
	  # where sfile has been removed.  For example, suppose that sdir
	  # had been a CVS-controlled directory which was then removed
	  # by removing each file (and using update -P or some such).  Then
	  # suppose that the build process creates an sdir directory which
	  # is not supposed to be under CVS.
	  rm -r sdir
	  dotest conflicts3-24 "${testcvs} -q update -d sdir" "U sdir/sfile"
	  rm sdir/sfile
	  dotest conflicts3-25 "${testcvs} rm sdir/sfile" \
"${PROG} remove: scheduling .sdir/sfile. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest conflicts3-26 "${testcvs} ci -m remove sdir/sfile" \
"Removing sdir/sfile;
${CVSROOT_DIRNAME}/first-dir/sdir/sfile,v  <--  sfile
new revision: delete; previous revision: 1\.1
done"
	  rm -r sdir/CVS
	  dotest conflicts3-27 "${testcvs} -q update" "${QUESTION} sdir"
	  dotest conflicts3-28 "${testcvs} -q update -PdA" \
"${QUESTION} sdir"

	  cd ../..

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	conflicts4)
	  mkdir conflicts4; cd conflicts4
	  mkdir 1; cd 1
	  dotest conflicts4-1 "$testcvs -q co -l ."
	  mkdir first-dir
	  dotest conflicts4-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd ..
	  mkdir 2; cd 2
	  dotest conflicts4-3 "${testcvs} -q co -l first-dir" ''
	  cd ../1/first-dir
	  echo baseline >file1
	  dotest conflicts4-4 "${testcvs} -q add file1" \
"$PROG add: use .$PROG commit. to add this file permanently"
	  dotest conflicts4-5 "${testcvs} -q ci -m add-it" \
"RCS file: $CVSROOT_DIRNAME/first-dir/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ../../2/first-dir
	  dotest conflicts4-6 "${testcvs} -q update" "U file1"
	  # Make a local change
	  echo wibble2 >> file1
	  dotest conflicts4-7 "${testcvs} -q ci -m update2" \
"Checking in file1;
$CVSROOT_DIRNAME/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../../1/first-dir
	  echo wibble1 >>file1
	  dotest conflicts4-8 "${testcvs} -Q update" \
"RCS file: $CVSROOT_DIRNAME/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into file1
rcsmerge: warning: conflicts during merge
cvs update: conflicts found in file1"
	  dotest_fail conflicts4-9 "${testcvs} -q update" \
"C file1"

	  if $remote; then
	    cat >$TESTDIR/conflicts4/serveme <<EOF
#!$TESTSHELL
# This is admittedly a bit cheezy, in the sense that we make lots
# of assumptions about what the client is going to send us.
# We don't mention Repository, because current clients don't require it.
# Sending these at our own pace, rather than waiting for the client to
# make the requests, is bogus, but hopefully we can get away with it.
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update Global_option"
echo "ok"
echo "MT text C "
echo "MT fname file1"
echo "MT newline"
echo "error  "
cat >$TESTDIR/conflicts4/client.out
EOF
	    # Cygwin.  Pthffffffffft!
	    if test -n "$remotehost"; then
	      $CVS_RSH $remotehost "chmod +x $TESTDIR/conflicts4/serveme"
	    else
	      chmod +x $TESTDIR/conflicts4/serveme
	    fi
	    save_CVS_SERVER=$CVS_SERVER
	    CVS_SERVER=$TESTDIR/conflicts4/serveme; export CVS_SERVER
	    dotest_fail conflicts4-10r "$testcvs -q up" "C file1"
	    dotest conflicts4-11r "cat $TESTDIR/conflicts4/client.out" \
"$DOTSTAR
Argument --
Directory .
$CVSROOT_DIRNAME/first-dir
Entry /file1/1.2/$PLUS=//
Modified file1
u=.*,g=.*,o=.*
59
baseline
""<<<<<<< file1
wibble1
""=======
wibble2
"">>>>>>> 1.2
update"

	    cat >$TESTDIR/conflicts4/serveme <<EOF
#!$TESTSHELL
# This is admittedly a bit cheezy, in the sense that we make lots
# of assumptions about what the client is going to send us.
# We don't mention Repository, because current clients don't require it.
# Sending these at our own pace, rather than waiting for the client to
# make the requests, is bogus, but hopefully we can get away with it.
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update Global_option Empty-conflicts"
echo "ok"
echo "MT text C "
echo "MT fname file1"
echo "MT newline"
echo "error  "
cat >$TESTDIR/conflicts4/client.out
EOF

	    dotest_fail conflicts4-12r "$testcvs -q up" "C file1"
	    dotest conflicts4-13r "cat $TESTDIR/conflicts4/client.out" \
"$DOTSTAR
Argument --
Directory .
$CVSROOT_DIRNAME/first-dir
Entry /file1/1.2/$PLUS=//
Unchanged file1
update"

	    CVS_SERVER=$save_CVS_SERVER; export CVS_SERVER
	  fi

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
            exit 0
	  fi

	  cd ../../..
	  rm -rf conflicts4
	  rm -rf $CVSROOT_DIRNAME/first-dir
	  ;;

	clean)
	  # Test update -C (overwrite local mods w/ repository copies)
	  mkdir 1; cd 1
	  dotest clean-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest clean-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo "The usual boring test text." > cleanme.txt
          dotest clean-3 "${testcvs} add cleanme.txt" \
"${PROG} add: scheduling file .cleanme\.txt. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest clean-4 "${testcvs} -q ci -m clean-3" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/cleanme\.txt,v
done
Checking in cleanme\.txt;
${CVSROOT_DIRNAME}/first-dir/cleanme\.txt,v  <--  cleanme\.txt
initial revision: 1\.1
done"
          # Okay, preparation is done, now test.
          # Check that updating an unmodified copy works.
	  dotest clean-5 "${testcvs} -q update" ''
          # Check that updating -C an unmodified copy works.
	  dotest clean-6 "${testcvs} -q update -C" ''
          # Check that updating a modified copy works.
	  echo "fish" >> cleanme.txt
	  dotest clean-7 "${testcvs} -q update" 'M cleanme\.txt'
          # Check that updating -C a modified copy works.
	  dotest clean-8 "${testcvs} -q update -C" \
"(Locally modified cleanme\.txt moved to \.#cleanme\.txt\.1\.1)
U cleanme\.txt"
	  # And check that the backup copy really was made.
	  dotest clean-9 "cat .#cleanme.txt.1.1" \
"The usual boring test text\.
fish"

          # Do it all again, this time naming the file explicitly.
	  rm .#cleanme.txt.1.1
	  dotest clean-10 "${testcvs} -q update cleanme.txt" ''
	  dotest clean-11 "${testcvs} -q update -C cleanme.txt" ''
	  echo "bluegill" >> cleanme.txt
	  dotest clean-12 "${testcvs} -q update cleanme.txt" 'M cleanme\.txt'
	  dotest clean-13 "${testcvs} -q update -C cleanme.txt" \
"(Locally modified cleanme\.txt moved to \.#cleanme\.txt\.1\.1)
U cleanme\.txt"
	  # And check that the backup copy really was made.
	  dotest clean-14 "cat .#cleanme.txt.1.1" \
"The usual boring test text\.
bluegill"

	  # Now try with conflicts
	  cd ..
	  dotest clean-15 "${testcvs} -q co -d second-dir first-dir" \
'U second-dir/cleanme\.txt'
	  cd second-dir
	  echo "conflict test" >> cleanme.txt
	  dotest clean-16 "${testcvs} -q ci -m." \
"Checking in cleanme\.txt;
${CVSROOT_DIRNAME}/first-dir/cleanme\.txt,v  <--  cleanme\.txt
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../first-dir
	  echo "fish" >> cleanme.txt
	  dotest clean-17 "${testcvs} -nq update" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/cleanme\.txt,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into cleanme\.txt
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in cleanme\.txt
C cleanme\.txt"
	  dotest clean-18 "${testcvs} -q update -C" \
"(Locally modified cleanme\.txt moved to \.#cleanme\.txt\.1\.1)
U cleanme\.txt"
	  dotest clean-19 "cat .#cleanme.txt.1.1" \
"The usual boring test text\.
fish"
	  
          # Done.  Clean up.
	  cd ../..
          rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules)
	  # Tests of various ways to define and use modules.
	  # Roadmap to various modules tests:
	  # -a:
	  #   error on incorrect placement: modules
	  #   error combining with other options: modules2-a*
	  #   infinite loops: modules148a1.1 - modules148a1.2
	  #   use to specify a file more than once: modules3
	  #   use with ! feature: modules4
	  # regular modules: modules, modules2, cvsadm
	  # ampersand modules: modules2
	  # -s: modules.
	  # -d: modules, modules3, cvsadm
	  # -i, -o, -u, -e, -t: modules5
	  # slashes in module names: modules3
	  # invalid module definitions: modules6

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
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
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
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
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
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/somedir
	  ############################################################
	  # end rebuild tests
	  ############################################################


	  mkdir ${CVSROOT_DIRNAME}/first-dir

	  mkdir 1
	  cd 1

	  dotest modules-143 "${testcvs} -q co first-dir" ""

	  cd first-dir
	  mkdir subdir
	  dotest modules-143a "${testcvs} add subdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir added to the repository"

	  cd subdir
	  mkdir ssdir
	  dotest modules-143b "${testcvs} add ssdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir/ssdir added to the repository"

	  touch a b

	  dotest modules-144 "${testcvs} add a b" \
"${PROG} add: scheduling file .a. for addition
${PROG} add: scheduling file .b. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"

	  dotest modules-145 "${testcvs} ci -m added" \
"${PROG} [a-z]*: Examining .
${PROG} [a-z]*: Examining ssdir
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/a,v
done
Checking in a;
${CVSROOT_DIRNAME}/first-dir/subdir/a,v  <--  a
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/b,v
done
Checking in b;
${CVSROOT_DIRNAME}/first-dir/subdir/b,v  <--  b
initial revision: 1\.1
done"

	  cd ..
	  dotest modules-146 "${testcvs} -q co CVSROOT" \
"U CVSROOT/checkoutlist
U CVSROOT/commitinfo
U CVSROOT/config
U CVSROOT/cvswrappers
U CVSROOT/editinfo
U CVSROOT/loginfo
U CVSROOT/modules
U CVSROOT/notify
U CVSROOT/rcsinfo
U CVSROOT/taginfo
U CVSROOT/verifymsg"

	  # Here we test that CVS can deal with CVSROOT (whose repository
	  # is at top level) in the same directory as subdir (whose repository
	  # is a subdirectory of first-dir).  TODO: Might want to check that
	  # files can actually get updated in this state.
	  dotest modules-147 "${testcvs} -q update" ""

	  cat >CVSROOT/modules <<EOF
realmodule first-dir/subdir a
dirmodule first-dir/subdir
namedmodule -d nameddir first-dir/subdir
aliasmodule -a first-dir/subdir/a
aliasnested -a first-dir/subdir/ssdir
topfiles -a first-dir/file1 first-dir/file2
world -a .
statusmod -s Mungeable
# Check for ability to block infinite loops.
infinitealias -a infinitealias
# Prior to 1.11.12 & 1.12.6, the infinite alias loop check didn't strip
# slashes or work if a module called a module which then called itself
# (A -> A was blocked, but not A -> B -> A or deeper).
infinitealias2 -a infinitealias2/
infinitealias3 -a infinitealias4/
infinitealias4 -a aliasmodule infinitealias5
infinitealias5 -a infinitealias3/
# Options must come before arguments.  It is possible this should
# be relaxed at some point (though the result would be bizarre for
# -a); for now test the current behavior.
bogusalias first-dir/subdir/a -a
EOF
	  dotest modules-148 "${testcvs} ci -m 'add modules' CVSROOT/modules" \
"Checking in CVSROOT/modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  cd ..
	  # The "statusmod" module contains an error; trying to use it
	  # will produce "modules file missing directory" I think.
	  # However, that shouldn't affect the ability of "cvs co -c" or
	  # "cvs co -s" to do something reasonable with it.
	  dotest modules-148a0 "${testcvs} co -c" 'aliasmodule  -a first-dir/subdir/a
aliasnested  -a first-dir/subdir/ssdir
bogusalias   first-dir/subdir/a -a
dirmodule    first-dir/subdir
infinitealias -a infinitealias
infinitealias2 -a infinitealias2/
infinitealias3 -a infinitealias4/
infinitealias4 -a aliasmodule infinitealias5
infinitealias5 -a infinitealias3/
namedmodule  -d nameddir first-dir/subdir
realmodule   first-dir/subdir a
statusmod    -s Mungeable
topfiles     -a first-dir/file1 first-dir/file2
world        -a \.'
	  # There is code in modules.c:save_d which explicitly skips
	  # modules defined with -a, which is why aliasmodule is not
	  # listed.
	  dotest modules-148a1 "${testcvs} co -s" \
'statusmod    Mungeable  
bogusalias   NONE        first-dir/subdir/a -a
dirmodule    NONE        first-dir/subdir
namedmodule  NONE        first-dir/subdir
realmodule   NONE        first-dir/subdir a'

	  # Check that infinite loops are avoided
	  dotest modules-148a1.1 "${testcvs} co infinitealias" \
"$PROG checkout: module \`infinitealias' in modules file contains infinite loop" \
"$PROG server: module \`infinitealias' in modules file contains infinite loop
$PROG checkout: module \`infinitealias' in modules file contains infinite loop"
	  # Prior to 1.11.12 & 1.12.6, the inifinte alias loop check did not
	  # strip slashes.
	  dotest modules-148a1.2 "${testcvs} co infinitealias2" \
"$PROG checkout: module \`infinitealias2' in modules file contains infinite loop" \
"$PROG server: module \`infinitealias2' in modules file contains infinite loop
$PROG checkout: module \`infinitealias2' in modules file contains infinite loop"
	  # Prior to 1.11.12 & 1.12.6, the inifinte alias loop check did not
	  # notice when A -> B -> A, it only noticed A -> A.
	  dotest modules-148a1.3 "${testcvs} co infinitealias3/" \
"$PROG checkout: module \`infinitealias3' in modules file contains infinite loop" \
"$PROG server: module \`infinitealias3' in modules file contains infinite loop
$PROG checkout: module \`infinitealias3' in modules file contains infinite loop"

	  # Test that real modules check out to realmodule/a, not subdir/a.
	  dotest modules-149a1 "${testcvs} co realmodule" "U realmodule/a"
	  dotest modules-149a2 "test -d realmodule && test -f realmodule/a" ""
	  dotest_fail modules-149a3 "test -f realmodule/b" ""
	  dotest modules-149a4 "${testcvs} -q co realmodule" ""
	  dotest modules-149a5 "echo yes | ${testcvs} release -d realmodule" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .realmodule.: "

	  dotest_fail modules-149b1 "${testcvs} co realmodule/a" \
"${PROG}"' checkout: module `realmodule/a'\'' is a request for a file in a module which is not a directory' \
"${PROG}"' server: module `realmodule/a'\'' is a request for a file in a module which is not a directory
'"${PROG}"' \[checkout aborted\]: cannot expand modules'

	  # Now test the ability to check out a single file from a directory
	  dotest modules-150c "${testcvs} co dirmodule/a" "U dirmodule/a"
	  dotest modules-150d "test -d dirmodule && test -f dirmodule/a" ""
	  dotest_fail modules-150e "test -f dirmodule/b" ""
	  dotest modules-150f "echo yes | ${testcvs} release -d dirmodule" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .dirmodule.: "
	  # Now test the ability to correctly reject a non-existent filename.
	  # For maximum studliness we would check that an error message is
	  # being output.
	  # We accept a zero exit status because it is what CVS does
	  # (Dec 95).  Probably the exit status should be nonzero,
	  # however.
	  dotest modules-150g1 "${testcvs} co dirmodule/nonexist" \
"${PROG} checkout: warning: new-born dirmodule/nonexist has disappeared"
	  # We tolerate the creation of the dirmodule directory, since that
	  # is what CVS does, not because we view that as preferable to not
	  # creating it.
	  dotest_fail modules-150g2 "test -f dirmodule/a || test -f dirmodule/b" ""
	  rm -r dirmodule

	  # Now test that a module using -d checks out to the specified
	  # directory.
	  dotest modules-150h1 "${testcvs} -q co namedmodule" \
'U nameddir/a
U nameddir/b'
	  dotest modules-150h2 "test -f nameddir/a && test -f nameddir/b" ""
	  echo add line >>nameddir/a
	  dotest modules-150h3 "${testcvs} -q co namedmodule" 'M nameddir/a'
	  rm nameddir/a
	  dotest modules-150h4 "${testcvs} -q co namedmodule" 'U nameddir/a'
	  dotest modules-150h99 "echo yes | ${testcvs} release -d nameddir" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .nameddir.: "

	  # Now test that alias modules check out to subdir/a, not
	  # aliasmodule/a.
	  dotest modules-151 "${testcvs} co aliasmodule" ""
	  dotest_fail modules-152 "test -d aliasmodule" ""
	  echo abc >>first-dir/subdir/a
	  dotest modules-153 "${testcvs} -q co aliasmodule" "M first-dir/subdir/a"

	  cd ..
	  rm -r 1

	  mkdir 2
	  cd 2
	  dotest modules-155a0 "${testcvs} co aliasnested" \
"${PROG} checkout: Updating first-dir/subdir/ssdir"
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
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: scheduling file `file2'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add these files permanently'
	  dotest modules-155c3 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
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
"${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest modules-155c7 "${testcvs} -q ci -m remove-it" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  cd ..
	  rm -r first-dir
	  dotest modules-155c8 "${testcvs} -q co topfiles" \
"${PROG} checkout: warning: first-dir/file1 is not (any longer) pertinent
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository
Directory ${CVSROOT_DIRNAME}/second-dir added to the repository
Directory ${CVSROOT_DIRNAME}/third-dir added to the repository"
	  cd third-dir
	  touch file3
	  dotest modules2-setup-3 "${testcvs} add file3" \
"${PROG} add: scheduling file .file3. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest modules2-setup-4 "${testcvs} -q ci -m add file3" \
"RCS file: ${CVSROOT_DIRNAME}/third-dir/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/third-dir/file3,v  <--  file3
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  mkdir 1
	  cd 1

	  dotest modules2-1 "${testcvs} -q co CVSROOT/modules" \
'U CVSROOT/modules'
	  cd CVSROOT
	  cat >> modules << EOF
ampermodule &first-dir &second-dir
combmodule third-dir file3 &first-dir
ampdirmod -d newdir &first-dir &second-dir
badmod -d newdir
messymod first-dir &messymodchild
messymodchild -d sdir/child second-dir
EOF
	  # Depending on whether the user also ran the modules test
	  # we will be checking in revision 1.2 or 1.3.
	  dotest modules2-2 "${testcvs} -q ci -m add-modules" \
"Checking in modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  cd ..

	  dotest modules2-3 "${testcvs} -q co ampermodule" ''
	  dotest modules2-4 "test -d ampermodule/first-dir" ''
	  dotest modules2-5 "test -d ampermodule/second-dir" ''

	  # Test ability of cvs release to handle multiple arguments
	  # See comment at "release" for list of other cvs release tests.
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

	  # There used to be a nasty-hack that made CVS skip creation of the
	  # module dir (in this case ampermodule) when -n was specified
	  dotest modules2-ampermod-1 "${testcvs} -q co -n ampermodule" ''
	  dotest modules2-ampermod-2 "test -d ampermodule/first-dir" ''
	  dotest modules2-ampermod-3 "test -d ampermodule/second-dir" ''

	  # Test release of a module
	  if echo yes |${testcvs} release -d ampermodule >>${LOGFILE}; then
	    pass modules2-ampermod-release-1
	  else
	    fail modules2-ampermod-release-1
	  fi
	  dotest_fail modules2-ampermod-release-2 "test -d ampermodule" ''

	  # and the '-n' test again, but in conjunction with '-d'
	  dotest modules2-ampermod-4 "${testcvs} -q co -n -d newname ampermodule" ''
	  dotest modules2-ampermod-5 "test -d newname/first-dir" ''
	  dotest modules2-ampermod-6 "test -d newname/second-dir" ''
	  rm -rf newname

	  # Now we create another directory named first-dir and make
	  # sure that CVS doesn't get them mixed up.
	  mkdir first-dir
	  # Note that this message should say "Updating ampermodule/first-dir"
	  # I suspect.  This is a long-standing behavior/bug....
	  dotest modules2-9 "${testcvs} co ampermodule" \
"${PROG} checkout: Updating first-dir
${PROG} checkout: Updating second-dir"
	  touch ampermodule/first-dir/amper1
	  cd ampermodule
	  dotest modules2-10 "${testcvs} add first-dir/amper1" \
"${PROG} add: scheduling file .first-dir/amper1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  cd ..

	  # As with the "Updating xxx" message, the "U first-dir/amper1"
	  # message (instead of "U ampermodule/first-dir/amper1") is
	  # rather fishy.
	  dotest modules2-12 "${testcvs} co ampermodule" \
"${PROG} checkout: Updating first-dir
A first-dir/amper1
${PROG} checkout: Updating second-dir"

	  if $remote; then
	    dotest modules2-13 "${testcvs} -q ci -m add-it ampermodule" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/amper1,v
done
Checking in ampermodule/first-dir/amper1;
${CVSROOT_DIRNAME}/first-dir/amper1,v  <--  amper1
initial revision: 1\.1
done"
	  else
	    # Trying this as above led to a "protocol error" message.
	    # Work around this bug.
	    cd ampermodule
	    dotest modules2-13 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/amper1,v
done
Checking in first-dir/amper1;
${CVSROOT_DIRNAME}/first-dir/amper1,v  <--  amper1
initial revision: 1\.1
done"
	    cd ..
	  fi
	  cd ..
	  rm -r 1

	  # Now test the "combmodule" module (combining regular modules
	  # and ampersand modules in the same module definition).
	  mkdir 1; cd 1
	  dotest modules2-14 "${testcvs} co combmodule" \
"U combmodule/file3
${PROG} checkout: Updating first-dir
U first-dir/amper1"
	  dotest modules2-15 "test -f combmodule/file3" ""
	  dotest modules2-16 "test -f combmodule/first-dir/amper1" ""
	  cd combmodule
	  rm -r first-dir
	  # At least for now there is no way to tell CVS that
	  # some files/subdirectories come from one repository directory,
	  # and others from another.
	  # This seems like a pretty sensible behavior to me, in the
	  # sense that first-dir doesn't "really" exist within
	  # third-dir, so CVS just acts as if there is nothing there
	  # to do.
	  dotest modules2-17 "${testcvs} update -d" \
"${PROG} update: Updating \."

	  cd ..
	  dotest modules2-18 "${testcvs} -q co combmodule" \
"U first-dir/amper1"
	  dotest modules2-19 "test -f combmodule/first-dir/amper1" ""
	  cd ..
	  rm -r 1

	  # Now test the "ampdirmod" and "badmod" modules to be sure that
	  # options work with ampersand modules but don't prevent the
	  # "missing directory" error message.
	  mkdir 1; cd 1
	  dotest modules2-20 "${testcvs} co ampdirmod" \
"${PROG} checkout: Updating first-dir
U first-dir/amper1
${PROG} checkout: Updating second-dir"
	  dotest modules2-21 "test -f newdir/first-dir/amper1" ""
	  dotest modules2-22 "test -d newdir/second-dir" ""
	  dotest_fail modules2-23 "${testcvs} co badmod" \
"${PROG} checkout: modules file missing directory for module badmod" \
"${PROG} server: modules file missing directory for module badmod
${PROG} \[checkout aborted\]: cannot expand modules"
	  cd ..
	  rm -r 1

	  # Confirm that a rename with added depth nested in an ampersand
	  # module works.
	  mkdir 1; cd 1
	  dotest modules2-nestedrename-1 "${testcvs} -q co messymod" \
"U messymod/amper1"
	  dotest modules2-nestedrename-2 "test -d messymod/sdir" ''
	  dotest modules2-nestedrename-3 "test -d messymod/sdir/CVS" ''
	  dotest modules2-nestedrename-4 "test -d messymod/sdir/child" ''
	  dotest modules2-nestedrename-5 "test -d messymod/sdir/child/CVS" ''
	  cd ..; rm -r 1

	  # FIXME:  client/server has a bug.  It should be working like a local
	  # repository in this case, but fails to check out the second module
	  # in the list when a branch is specified.
	  mkdir 1; cd 1
	  dotest modules2-ampertag-setup-1 \
"${testcvs} -Q rtag tag first-dir second-dir third-dir" \
''
	  dotest modules2-ampertag-1 "${testcvs} -q co -rtag ampermodule" \
"U first-dir/amper1"
	  if $remote; then
	    dotest_fail modules2-ampertag-2 "test -d ampermodule/second-dir" ''
	    dotest_fail modules2-ampertag-3 "test -d ampermodule/second-dir/CVS" ''
	  else
	    dotest modules2-ampertag-2 "test -d ampermodule/second-dir" ''
	    dotest modules2-ampertag-3 "test -d ampermodule/second-dir/CVS" ''
	  fi
	  cd ..; rm -r 1

	  # Test for tag files when an ampermod is renamed with more path
	  # elements than it started with.
	  #
	  # FIXME: This is currently broken in the remote case, possibly only
	  # because the messymodchild isn't being checked out at all.
	  mkdir 1; cd 1
#	  dotest modules2-tagfiles-setup-1 \
#"${testcvs} -Q rtag -b branch first-dir second-dir" \
#''
	  dotest modules2-tagfiles-1 "${testcvs} -q co -rtag messymod" \
"U messymod/amper1"
	  if $remote; then
	    dotest_fail modules2-tagfiles-2r "test -d messymod/sdir" ''
	  else
	    dotest modules2-tagfiles-2 "cat messymod/sdir/CVS/Tag" 'Ttag'
	  fi
	  cd ..; rm -r 1

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
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..
	  dotest_fail modules2-a1 "${testcvs} -q co aliasopt" \
"${PROG} checkout: -a cannot be specified in the modules file along with other options" \
"${PROG} server: -a cannot be specified in the modules file along with other options
${PROG} \[checkout aborted\]: cannot expand modules"
	  cd ..;  rm -r 1

	  # Clean up.
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"

	  cd first-dir
	  echo file1 >file1
	  dotest modules3-2 "${testcvs} add file1" \
"${PROG} add: scheduling file \`file1' for addition
${PROG} add: use '${PROG} commit' to add this file permanently"
	  dotest modules3-3 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
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
"${PROG} import: Importing ${CVSROOT_DIRNAME}/second-dir/suba
${PROG} import: Importing ${CVSROOT_DIRNAME}/second-dir/suba/subb

No conflicts created by this import" "
No conflicts created by this import"
	  cd ..; rm -r 1
	  mkdir 1; cd 1
	  dotest modules3-7b "${testcvs} co second-dir" \
"${PROG} checkout: Updating second-dir
${PROG} checkout: Updating second-dir/suba
${PROG} checkout: Updating second-dir/suba/subb" \
"${PROG} checkout: Updating second-dir"

	  if $remote; then
	    cd second-dir
	    mkdir suba
	    dotest modules3-7-workaround1 "${testcvs} add suba" \
"Directory ${CVSROOT_DIRNAME}/second-dir/suba added to the repository"
	    cd suba
	    mkdir subb
	    dotest modules3-7-workaround2 "${testcvs} add subb" \
"Directory ${CVSROOT_DIRNAME}/second-dir/suba/subb added to the repository"
	    cd ../..
	  fi

	  cd second-dir/suba/subb
	  touch fileb
	  dotest modules3-7c "${testcvs} add fileb" \
"${PROG} add: scheduling file .fileb. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest modules3-7d "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/second-dir/suba/subb/fileb,v
done
Checking in fileb;
${CVSROOT_DIRNAME}/second-dir/suba/subb/fileb,v  <--  fileb
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
	  if $remote; then
	    dotest_fail modules3-11b \
"${testcvs} -q update ${TESTDIR}/1/src/sub1/sub2/sub3/dir/file1" \
"absolute pathname .${TESTDIR}/1/src/sub1/sub2/sub3/dir. illegal for server"
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
	  if $remote; then :; else
	    mkdir 1; cd 1
	    dotest modules3-12 "${testcvs} -q co path/in/modules" \
"U first-dir/file1"
	    dotest modules3-13 "test -f path/in/modules/first-dir/file1" ''
	    cd ..; rm -r 1
	  fi # end of tests skipped for remote

	  # Now here is where it used to get seriously bogus.
	  mkdir 1; cd 1
	  dotest modules3-14 \
"${testcvs} -q rtag tag1 path/in/modules" ''
	  # CVS used to create this even though rtag should *never* affect
	  # the directory current when it is called!
	  dotest_fail modules3-15 "test -d path/in/modules" ''
	  # Just for trivia's sake, rdiff was not similarly vulnerable
	  # because it passed 0 for run_module_prog to do_module.
	  cd ..; rm -r 1

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

	modules4)
	  # Some tests using the modules file with aliases that
	  # exclude particular directories.

	  mkdir 1; cd 1

	  dotest modules4-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest modules4-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"

	  cd first-dir
          mkdir subdir subdir_long
          dotest modules4-3 "${testcvs} add subdir subdir_long" \
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir added to the repository
Directory ${CVSROOT_DIRNAME}/first-dir/subdir_long added to the repository"

	  echo file1 > file1
	  dotest modules4-4 "${testcvs} add file1" \
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  echo file2 > subdir/file2
	  dotest modules4-5 "${testcvs} add subdir/file2" \
"${PROG}"' add: scheduling file `subdir/file2'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  echo file3 > subdir_long/file3
	  dotest modules4-6 "${testcvs} add subdir_long/file3" \
"${PROG}"' add: scheduling file `subdir_long/file3'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  dotest modules4-7 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/file2,v
done
Checking in subdir/file2;
${CVSROOT_DIRNAME}/first-dir/subdir/file2,v  <--  file2
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir_long/file3,v
done
Checking in subdir_long/file3;
${CVSROOT_DIRNAME}/first-dir/subdir_long/file3,v  <--  file3
initial revision: 1\.1
done"

	  cd ..

	  dotest modules4-8 "${testcvs} -q update -d CVSROOT" \
"U CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  cat >modules <<EOF
all -a first-dir
some -a !first-dir/subdir first-dir
other -a !first-dir/subdir !first-dir/subdir_long first-dir
somewhat -a first-dir !first-dir/subdir
EOF
	  dotest modules4-9 "${testcvs} -q ci -m add-modules" \
"Checking in modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..

	  cd ..
	  mkdir 2; cd 2

	  dotest modules4-10 "${testcvs} -q co all" \
"U first-dir/file1
U first-dir/subdir/file2
U first-dir/subdir_long/file3"
	  rm -r first-dir

	  dotest modules4-11 "${testcvs} -q co some" \
"U first-dir/file1
U first-dir/subdir_long/file3"
	  dotest_fail modules4-12 "test -d first-dir/subdir" ''
	  dotest modules4-13 "test -d first-dir/subdir_long" ''
	  rm -r first-dir

	  if $remote; then
	    # But remote seems to do it the other way.
	    dotest modules4-14r-1 "${testcvs} -q co somewhat" \
"U first-dir/file1
U first-dir/subdir_long/file3"
	    dotest_fail modules4-14r-2 "test -d first-dir/subdir" ''
	    dotest modules4-14r-3 "test -d first-dir/subdir_long" ''
	  else
	    # This is strange behavior, in that the order of the
	    # "!first-dir/subdir" and "first-dir" matter, and it isn't
	    # clear that they should.  I suspect it is long-standing
	    # strange behavior but I haven't verified that.
	    dotest modules4-14-1 "${testcvs} -q co somewhat" \
"U first-dir/file1
U first-dir/subdir/file2
U first-dir/subdir_long/file3"
	    dotest modules4-14-2 "test -d first-dir/subdir" ''
	    dotest modules4-14-3 "test -d first-dir/subdir_long" ''
	  fi
	  rm -r first-dir

	  dotest modules4-15 "${testcvs} -q co other" \
"U first-dir/file1"
	  dotest_fail modules4-16 "test -d first-dir/subdir" ''
	  dotest_fail modules4-17 "test -d first-dir/subdir_long" ''
	  rm -r first-dir

	  cd ..
	  rm -r 2

	  dotest modules4-18 "${testcvs} rtag tag some" \
"${PROG} rtag: Tagging first-dir
${PROG} rtag: Ignoring first-dir/subdir
${PROG} rtag: Tagging first-dir/subdir_long"

	  cd 1/first-dir/subdir
	  dotest modules4-19 "${testcvs} log file2" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/file2,v
Working file: file2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add-it
============================================================================="

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
            exit 0
	  fi

	  cd ../../..
	  rm -r 1

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modules5)
	  # Test module programs

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest modules5-1 "${testcvs} -q co first-dir" ""
	  cd first-dir
	  mkdir subdir
	  dotest modules5-2 "${testcvs} add subdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir added to the repository"
	  cd subdir
	  mkdir ssdir
	  dotest modules5-3 "${testcvs} add ssdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir/ssdir added to the repository"
	  touch a b
	  dotest modules5-4 "${testcvs} add a b" \
"${PROG} add: scheduling file .a. for addition
${PROG} add: scheduling file .b. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"

	  dotest modules5-5 "${testcvs} ci -m added" \
"${PROG} [a-z]*: Examining .
${PROG} [a-z]*: Examining ssdir
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/a,v
done
Checking in a;
${CVSROOT_DIRNAME}/first-dir/subdir/a,v  <--  a
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/b,v
done
Checking in b;
${CVSROOT_DIRNAME}/first-dir/subdir/b,v  <--  b
initial revision: 1\.1
done"

	  cd ..
	  dotest modules5-6 "${testcvs} -q co CVSROOT" \
"U CVSROOT/checkoutlist
U CVSROOT/commitinfo
U CVSROOT/config
U CVSROOT/cvswrappers
U CVSROOT/editinfo
U CVSROOT/loginfo
U CVSROOT/modules
U CVSROOT/notify
U CVSROOT/rcsinfo
U CVSROOT/taginfo
U CVSROOT/verifymsg"

	  # FIXCVS: The sleep in the following script helps avoid out of
	  # order messages, but we really need to figure out how to fix
	  # cvs to prevent them in the first place.
	  for i in checkout export tag; do
	    cat >> ${CVSROOT_DIRNAME}/$i.sh <<EOF
#! /bin/sh
sleep 1
echo "$i script invoked in \`pwd\`"
echo "args: \$@"
EOF
	    # Cygwin doesn't set premissions correctly over the Samba share.
	    if test -n "$remotehost"; then
	      $CVS_RSH $remotehost "chmod +x ${CVSROOT_DIRNAME}/$i.sh"
	    else
	      chmod +x ${CVSROOT_DIRNAME}/$i.sh
	    fi
	  done

	  OPTS="-o${CVSROOT_DIRNAME}/checkout.sh -e ${CVSROOT_DIRNAME}/export.sh -t${CVSROOT_DIRNAME}/tag.sh"
	  cat >CVSROOT/modules <<EOF
realmodule ${OPTS} first-dir/subdir a
dirmodule ${OPTS} first-dir/subdir
namedmodule -d nameddir ${OPTS} first-dir/subdir
EOF

	  dotest modules5-7 "${testcvs} ci -m 'add modules' CVSROOT/modules" \
"" \
"Checking in CVSROOT/modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  cd ..
	  rm -rf first-dir

	  # Test that real modules check out to realmodule/a, not subdir/a.
	  if $remote; then
	    # FIXCVS?
	    # Mac OSX 10.3 (Darwin ppc-osx1 5.5) fails here when $TMPDIR
	    # contains a symlink (it does not fail the local modules5-8).
	    # Since no other platforms are exhibiting the same problem, I
	    # suspect an issue with OSX and fork() or the like dereferencing
	    # the symlink, but it is possible it is something that could be
	    # fixed or worked around in CVS.
	    dotest modules5-8r "$testcvs co realmodule" \
"U realmodule/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .realmodule..
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: realmodule"
	  else
	    dotest modules5-8 "${testcvs} co realmodule" \
"U realmodule/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .realmodule..
checkout script invoked in ${TESTDIR}/1
args: realmodule"
	  fi
	  dotest modules5-9 "test -d realmodule && test -f realmodule/a" ""
	  dotest_fail modules5-10 "test -f realmodule/b" ""
	  if $remote; then
	    dotest modules5-11 "${testcvs} -q co realmodule" \
"checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: realmodule"
	    dotest modules5-12 "${testcvs} -q update" ''
	    echo "change" >>realmodule/a
	    dotest modules5-13 "${testcvs} -q ci -m." \
"Checking in realmodule/a;
${CVSROOT_DIRNAME}/first-dir/subdir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done"
	  else
	    dotest modules5-11 "${testcvs} -q co realmodule" \
"checkout script invoked in ${TESTDIR}/1
args: realmodule"
	    dotest modules5-12 "${testcvs} -q update" ''
	    echo "change" >>realmodule/a
	    dotest modules5-13 "${testcvs} -q ci -m." \
"Checking in realmodule/a;
${CVSROOT_DIRNAME}/first-dir/subdir/a,v  <--  a
new revision: 1\.2; previous revision: 1\.1
done"
	  fi
	  dotest modules5-14 "echo yes | ${testcvs} release -d realmodule" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .realmodule.: "
	  dotest modules5-15 "${testcvs} -q rtag -Dnow MYTAG realmodule" \
"tag script invoked in ${TESTDIR}/1
args: realmodule MYTAG" \
"tag script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: realmodule MYTAG"
	  if $remote; then
	    dotest modules5-16 "${testcvs} -q export -r MYTAG realmodule" \
"U realmodule/a
export script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: realmodule"
	  else
	    dotest modules5-16 "${testcvs} -q export -r MYTAG realmodule" \
"U realmodule/a
export script invoked in ${TESTDIR}/1
args: realmodule"
	  fi
	  rm -r realmodule

	  dotest_fail modules5-17 "${testcvs} co realmodule/a" \
"${PROG}"' checkout: module `realmodule/a'\'' is a request for a file in a module which is not a directory' \
"${PROG}"' server: module `realmodule/a'\'' is a request for a file in a module which is not a directory
'"${PROG}"' \[checkout aborted\]: cannot expand modules'

	  # Now test the ability to check out a single file from a directory
	  if $remote; then
	    dotest modules5-18 "${testcvs} co dirmodule/a" \
"U dirmodule/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .dirmodule..
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: dirmodule"
	  else
	    dotest modules5-18 "${testcvs} co dirmodule/a" \
"U dirmodule/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .dirmodule..
checkout script invoked in ${TESTDIR}/1
args: dirmodule"
	  fi
	  dotest modules5-19 "test -d dirmodule && test -f dirmodule/a" ""
	  dotest_fail modules5-20 "test -f dirmodule/b" ""
	  dotest modules5-21 "echo yes | ${testcvs} release -d dirmodule" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .dirmodule.: "

	  # Now test the ability to correctly reject a non-existent filename.
	  # For maximum studliness we would check that an error message is
	  # being output.
	  # We accept a zero exit status because it is what CVS does
	  # (Dec 95).  Probably the exit status should be nonzero,
	  # however.
	  if $remote; then
	    dotest modules5-22 "${testcvs} co dirmodule/nonexist" \
"${PROG} checkout: warning: new-born dirmodule/nonexist has disappeared
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .dirmodule..
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: dirmodule"
	  else
	    dotest modules5-22 "${testcvs} co dirmodule/nonexist" \
"${PROG} checkout: warning: new-born dirmodule/nonexist has disappeared
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .dirmodule..
checkout script invoked in ${TESTDIR}/1
args: dirmodule"
	  fi
	  # We tolerate the creation of the dirmodule directory, since that
	  # is what CVS does, not because we view that as preferable to not
	  # creating it.
	  dotest_fail modules5-23 "test -f dirmodule/a || test -f dirmodule/b" ""
	  rm -r dirmodule

	  # Now test that a module using -d checks out to the specified
	  # directory.
	  if $remote; then
	    dotest modules5-24 "${testcvs} -q co namedmodule" \
"U nameddir/a
U nameddir/b
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: nameddir"
	  else
	    dotest modules5-24 "${testcvs} -q co namedmodule" \
"U nameddir/a
U nameddir/b
checkout script invoked in ${TESTDIR}/1
args: nameddir"
	  fi
	  dotest modules5-25 "test -f nameddir/a && test -f nameddir/b" ""
	  echo add line >>nameddir/a
	  # This seems suspicious: when we checkout an existing directory,
	  # the checkout script gets executed in addition to the update
	  # script.  Is that by design or accident?
	  if $remote; then
	    dotest modules5-26 "${testcvs} -q co namedmodule" \
"M nameddir/a
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: nameddir"
	  else
	    dotest modules5-26 "${testcvs} -q co namedmodule" \
"M nameddir/a
checkout script invoked in ${TESTDIR}/1
args: nameddir"
	  fi
	  rm nameddir/a

	  if $remote; then
	    dotest modules5-27 "${testcvs} -q co namedmodule" \
"U nameddir/a
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: nameddir"
	  else
	    dotest modules5-27 "${testcvs} -q co namedmodule" \
"U nameddir/a
checkout script invoked in ${TESTDIR}/1
args: nameddir"
	  fi
	  dotest modules5-28 "echo yes | ${testcvs} release -d nameddir" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .nameddir.: "

	  # Now try the same tests with -d on command line
	  # FIXCVS?  The manual says the modules programs get the module name,
	  # but they really get the directory name.
	  if $remote; then
	    dotest modules5-29 "${testcvs} co -d mydir realmodule" \
"U mydir/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .mydir..
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-29 "${testcvs} co -d mydir realmodule" \
"U mydir/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .mydir..
checkout script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  dotest modules5-30 "test -d mydir && test -f mydir/a" ""
	  dotest_fail modules5-31 "test -d realmodule || test -f mydir/b" ""
	  if $remote; then
	    dotest modules5-32 "${testcvs} -q co -d mydir realmodule" \
"checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	    dotest modules5-33 "${testcvs} -q update" ''
	    echo "change" >>mydir/a
	    dotest modules5-34 "${testcvs} -q ci -m." \
"Checking in mydir/a;
${CVSROOT_DIRNAME}/first-dir/subdir/a,v  <--  a
new revision: 1\.3; previous revision: 1\.2
done"
	  else
	    dotest modules5-32 "${testcvs} -q co -d mydir realmodule" \
"checkout script invoked in ${TESTDIR}/1
args: mydir"
	    dotest modules5-33 "${testcvs} -q update" ''
	    echo "change" >>mydir/a
	    dotest modules5-34 "${testcvs} -q ci -m." \
"Checking in mydir/a;
${CVSROOT_DIRNAME}/first-dir/subdir/a,v  <--  a
new revision: 1\.3; previous revision: 1\.2
done"
	  fi
	  dotest modules5-35 "echo yes | ${testcvs} release -d mydir" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .mydir.: "
	  if $remote; then
	    dotest modules5-36 "${testcvs} -q rtag -Dnow MYTAG2 realmodule" \
"tag script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: realmodule MYTAG2"
	    dotest modules5-37 "${testcvs} -q export -r MYTAG2 -d mydir realmodule" \
"U mydir/a
export script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-36 "${testcvs} -q rtag -Dnow MYTAG2 realmodule" \
"tag script invoked in ${TESTDIR}/1
args: realmodule MYTAG2"
	    dotest modules5-37 "${testcvs} -q export -r MYTAG2 -d mydir realmodule" \
"U mydir/a
export script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  rm -r mydir

	  # Now test the ability to check out a single file from a directory
	  if $remote; then
	    dotest modules5-38 "${testcvs} co -d mydir dirmodule/a" \
"U mydir/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .mydir..
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-38 "${testcvs} co -d mydir dirmodule/a" \
"U mydir/a
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .mydir..
checkout script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  dotest modules5-39 "test -d mydir && test -f mydir/a" ""
	  dotest_fail modules5-40 "test -d dirmodule || test -f mydir/b" ""
	  dotest modules5-41 "echo yes | ${testcvs} release -d mydir" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .mydir.: "

	  # Now test the ability to correctly reject a non-existent filename.
	  # For maximum studliness we would check that an error message is
	  # being output.
	  # We accept a zero exit status because it is what CVS does
	  # (Dec 95).  Probably the exit status should be nonzero,
	  # however.
	  if $remote; then
	    dotest modules5-42 "${testcvs} co -d mydir dirmodule/nonexist" \
"${PROG} checkout: warning: new-born mydir/nonexist has disappeared
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .mydir..
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-42 "${testcvs} co -d mydir dirmodule/nonexist" \
"${PROG} checkout: warning: new-born mydir/nonexist has disappeared
${PROG} checkout: Executing ..${CVSROOT_DIRNAME}/checkout\.sh. .mydir..
checkout script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  # We tolerate the creation of the mydir directory, since that
	  # is what CVS does, not because we view that as preferable to not
	  # creating it.
	  dotest_fail modules5-43 "test -f mydir/a || test -f mydir/b" ""
	  rm -r mydir

	  if $remote; then
	    dotest modules5-44 "${testcvs} -q co -d mydir namedmodule" \
"U mydir/a
U mydir/b
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-44 "${testcvs} -q co -d mydir namedmodule" \
"U mydir/a
U mydir/b
checkout script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  dotest modules5-45 "test -f mydir/a && test -f mydir/b" ""
	  dotest_fail modules5-46 "test -d namedir"
	  echo add line >>mydir/a
	  # This seems suspicious: when we checkout an existing directory,
	  # the checkout script gets executed in addition to the update
	  # script.  Is that by design or accident?
	  if $remote; then
	    dotest modules5-47 "${testcvs} -q co -d mydir namedmodule" \
"M mydir/a
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-47 "${testcvs} -q co -d mydir namedmodule" \
"M mydir/a
checkout script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  rm mydir/a

	  if $remote; then
	    dotest modules5-48 "${testcvs} -q co -d mydir namedmodule" \
"U mydir/a
checkout script invoked in ${TMPDIR}/cvs-serv[0-9a-z]*
args: mydir"
	  else
	    dotest modules5-48 "${testcvs} -q co -d mydir namedmodule" \
"U mydir/a
checkout script invoked in ${TESTDIR}/1
args: mydir"
	  fi
	  dotest modules5-49 "echo yes | ${testcvs} release -d mydir" \
"You have \[0\] altered files in this repository\.
Are you sure you want to release (and delete) directory .mydir.: "

	  cd ..
	  rm -rf 1 ${CVSROOT_DIRNAME}/first-dir ${CVSROOT_DIRNAME}/*.sh
	  ;;

	modules6)
	  #
	  # Test invalid module definitions
	  #
	  # See the header comment for the `modules' test for an index of
	  # the complete suite of modules tests.
	  #

	  #
	  # There was a bug in CVS through 1.11.1p1 where a bad module name
	  # would cause the previous line to be parsed as the module
	  # definition.  This test proves this doesn't happen anymore.
	  #
	  mkdir modules6
	  cd modules6
	  dotest module6-setup-1 "${testcvs} -Q co CVSROOT" ""
	  cd CVSROOT
	  echo "longmodulename who cares" >modules
	  echo "badname" >>modules
	  # This test almost isn't setup since it generates the error message
	  # we are looking for if `-Q' isn't specified, but I want to test the
	  # filename in the message later.
	  dotest modules6-setup-2 "${testcvs} -Q ci -mbad-modules" \
"Checking in modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: [0-9.]*; previous revision: [0-9.]*
done
${PROG} commit: Rebuilding administrative file database"

	  # Here's where CVS would report not being able to find `lename'
	  cd ..
	  dotest_fail modules6-1 "${testcvs} -q co badname" \
"${PROG} checkout: warning: NULL value for key .badname. at line 2 of .${CVSROOT_DIRNAME}/CVSROOT/modules.
${PROG} checkout: cannot find module .badname. - ignored" \
"${PROG} server: warning: NULL value for key .badname. at line 2 of .${CVSROOT_DIRNAME}/CVSROOT/modules.
${PROG} server: cannot find module .badname. - ignored
${PROG} \[checkout aborted\]: cannot expand modules"

	  # cleanup
	  cd CVSROOT
	  echo "# empty modules file" >modules
	  dotest modules6-cleanup-1 "${testcvs} -Q ci -mempty-modules" \
"Checking in modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: [0-9.]*; previous revision: [0-9.]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..

	  if $keep; then :; else
	    rm -r modules6
	  fi
	  ;;



	modules7)
	  #
	  # Test tag problems vs an empty CVSROOT/val-tags file
	  #
	  # See the header comment for the `modules' test for an index of
	  # the complete suite of modules tests.
	  #
	  mkdir modules7
	  cd modules7
	  dotest modules7-1 "$testcvs -Q co -d top ."
	  cd top
	  mkdir zero one
	  dotest modules7-2 "$testcvs -Q add zero one"
	  cd one
	  echo 'file1 contents' > file1
	  dotest modules7-2 "$testcvs -Q add file1"
	  dotest modules7-3 "$testcvs -Q ci -mnew file1" \
"RCS file: $CVSROOT_DIRNAME/one/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/one/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest modules7-4 "$testcvs -Q tag mytag file1"
	  cd ../CVSROOT
	  echo 'all -a zero one' > modules
	  dotest modules7-5 "$testcvs -Q ci -mall-module" \
"Checking in modules;
$CVSROOT_DIRNAME/CVSROOT/modules,v  <--  modules
new revision: [0-9.]*; previous revision: [0-9.]*
done
$PROG commit: Rebuilding administrative file database"
	  cd ../..
	  mkdir myexport
	  cd myexport
	  # FIXCVS: The export should NOT be aborted here
	  dotest_fail modules7-6 "$testcvs export -rmytag all" \
"$PROG \[export aborted\]: no such tag mytag"
	  cd ..
	  rm -fr myexport
	  mkdir myexport
	  cd myexport
	  # FIXCVS: Workaround is to have mytag listed in val-tags
	  echo 'mytag y' > $CVSROOT_DIRNAME/CVSROOT/val-tags
	  dotest modules7-7 "$testcvs export -rmytag all" \
"$PROG export: Updating zero
$PROG export: Updating one
U one/file1"
	  dotest modules7-8 'cat one/file1' 'file1 contents'

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  # cleanup
	  cd ../top/CVSROOT
	  echo "# empty modules file" >modules
	  dotest modules7-cleanup-1 "$testcvs -Q ci -mempty-modules" \
"Checking in modules;
$CVSROOT_DIRNAME/CVSROOT/modules,v  <--  modules
new revision: [0-9.]*; previous revision: [0-9.]*
done
$PROG commit: Rebuilding administrative file database"
	  cd ../../..
	  rm -fr modules7
	  rm -rf $CVSROOT_DIRNAME/zero $CVSROOT_DIRNAME/one
	  ;;


	mkmodules)
	  # When a file listed in checkoutlist doesn't exist, cvs-1.10.4
	  # would fail to remove the CVSROOT/.#[0-9]* temporary file it
	  # creates while mkmodules is in the process of trying to check
	  # out the missing file.

	  mkdir 1; cd 1
	  dotest mkmodules-temp-file-removal-1 "${testcvs} -Q co CVSROOT" ''
	  cd CVSROOT
	  echo no-such-file >> checkoutlist
	  dotest mkmodules-temp-file-removal-2 "${testcvs} -Q ci -m. checkoutlist" \
"Checking in checkoutlist;
$CVSROOT_DIRNAME/CVSROOT/checkoutlist,v  <--  checkoutlist
new revision: 1\.2; previous revision: 1\.1
done
${PROG} commit: Rebuilding administrative file database"

	  dotest mkmodules-temp-file-removal-3 "echo $CVSROOT_DIRNAME/CVSROOT/.#[0-9]*" \
	    "$CVSROOT_DIRNAME/CVSROOT/\.#\[0-9\]\*"

	  # Versions 1.11.6 & 1.12.1 and earlier of CVS printed most of the
	  # white space included before error messages in checkoutlist.
	  echo "no-such-file     Failed to update no-such-file." >checkoutlist
	  dotest mkmodules-error-message-1 "${testcvs} -Q ci -m. checkoutlist" \
"Checking in checkoutlist;
$CVSROOT_DIRNAME/CVSROOT/checkoutlist,v  <--  checkoutlist
new revision: 1\.3; previous revision: 1\.2
done
${PROG} commit: Rebuilding administrative file database
${PROG} commit: Failed to update no-such-file\."

	  # Versions 1.11.6 & 1.12.1 and earlier of CVS used the error string
	  # from the checkoutlist file as the format string passed to error()'s
	  # printf.  Check that this is no longer the case by verifying that
	  # printf format patterns remain unchanged.
	  echo "no-such-file     Failed to update %s %lx times because %s happened %d times." >checkoutlist
	  dotest mkmodules-error-message-2 "${testcvs} -Q ci -m. checkoutlist" \
"Checking in checkoutlist;
$CVSROOT_DIRNAME/CVSROOT/checkoutlist,v  <--  checkoutlist
new revision: 1\.4; previous revision: 1\.3
done
${PROG} commit: Rebuilding administrative file database
${PROG} commit: Failed to update %s %lx times because %s happened %d times\."

	  dotest mkmodules-cleanup-1 "${testcvs} -Q up -pr1.1 checkoutlist >checkoutlist"
	  dotest mkmodules-cleanup-2 "${testcvs} -Q ci -m. checkoutlist" \
"Checking in checkoutlist;
$CVSROOT_DIRNAME/CVSROOT/checkoutlist,v  <--  checkoutlist
new revision: 1\.5; previous revision: 1\.4
done
${PROG} commit: Rebuilding administrative file database"

	  cd ../..
	  rm -rf 1
	  ;;

	co-d)
	  # Some tests of various permutations of co-d when directories exist
	  # and checkouts lengthen.
	  #
	  # Interestingly enough, these same tests pass when the directory
	  # lengthening happens via the modules file.  Go figure.
	  module=co-d
	  mkdir $module; cd $module
	  mkdir top; cd top
	  dotest co-d-init-1 "$testcvs -Q co -l ."
	  mkdir $module
	  dotest co-d-init-2 "$testcvs -Q add $module"
	  cd $module
	  echo content >file1
	  echo different content >file2
	  dotest co-d-init-3 "$testcvs -Q add file1 file2"
	  dotest co-d-init-4 "$testcvs -Q ci -madd-em" \
"RCS file: $CVSROOT_DIRNAME/co-d/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/co-d/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: $CVSROOT_DIRNAME/co-d/file2,v
done
Checking in file2;
$CVSROOT_DIRNAME/co-d/file2,v  <--  file2
initial revision: 1\.1
done"
	  cd ../..

	  mkdir 2; cd 2
	  dotest co-d-1 "$testcvs -q co -d dir $module" \
"U dir/file1
U dir/file2"
	  dotest co-d-1.2 "cat dir/CVS/Repository" "$module"

	  # FIXCVS: This should work.  Correct expected result:
	  #
	  #"U dir2/sdir/file1
	  #U dir2/sdir/file2"
	  dotest_fail co-d-2 "$testcvs -q co -d dir2/sdir $module" \
"$PROG \[checkout aborted\]: could not change directory to requested checkout directory \`dir2': No such file or directory"
	  # FIXCVS:
	  # dotest co-d-2.2 "cat dir4/CVS/Repository" "CVSROOT/Emptydir"
	  # dotest co-d-2.3 "cat dir5/CVS/Repository" "$module"

	  mkdir dir3
	  dotest co-d-3 "$testcvs -q co -d dir3 $module" \
"U dir3/file1
U dir3/file2"
	  dotest co-d-3.2 "cat dir3/CVS/Repository" "$module"

	  if $remote; then
	    # FIXCVS: As for co-d-2.
	    mkdir dir4
	    dotest_fail co-d-4r "$testcvs -q co -d dir4/sdir $module" \
"$PROG \[checkout aborted\]: could not change directory to requested checkout directory \`dir4': No such file or directory"

	    # FIXCVS: As for co-d-2.
	    mkdir dir5
	    mkdir dir5/sdir
	    dotest_fail co-d-5r "$testcvs -q co -d dir5/sdir $module" \
"$PROG \[checkout aborted\]: could not change directory to requested checkout directory \`dir5': No such file or directory"
	  else
	    mkdir dir4
	    dotest co-d-4 "$testcvs -q co -d dir4/sdir $module" \
"U dir4/sdir/file1
U dir4/sdir/file2"
	    # CVS only creates administration directories for directories it
	    # creates, and the last portion of the path passed to -d
	    # regardless.
	    dotest_fail co-d-4.2 "test -d dir4/CVS"
	    dotest co-d-4.3 "cat dir4/sdir/CVS/Repository" "$module"

	    mkdir dir5
	    mkdir dir5/sdir
	    dotest co-d-5 "$testcvs -q co -d dir5/sdir $module" \
"U dir5/sdir/file1
U dir5/sdir/file2"
	    # CVS only creates administration directories for directories it
	    # creates, and the last portion of the path passed to -d
	    # regardless.
	    dotest_fail co-d-5.2 "test -d dir5/CVS"
	    dotest co-d-5.3 "cat dir5/sdir/CVS/Repository" "$module"
	  fi

	  # clean up
	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -rf $CVSROOT_DIRNAME/$module
	  rm -r $module
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

	  # First, set TopLevelAdmin=yes so we're sure to get
	  # top-level CVS directories.
	  mkdir 1; cd 1
	  dotest cvsadm-setup-1 "${testcvs} -q co CVSROOT/config" \
"U CVSROOT/config"
	  cd CVSROOT
	  echo "TopLevelAdmin=yes" >config
	  dotest cvsadm-setup-2 "${testcvs} -q ci -m yes-top-level" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..
	  rm -r 1

	  # Second, check out the modules file and edit it.
	  mkdir 1; cd 1
	  dotest cvsadm-1 "${testcvs} co CVSROOT/modules" \
"U CVSROOT/modules"

	  # Test CVS/Root once.  Since there is only one part of
	  # the code which writes CVS/Root files (Create_Admin),
	  # there is no point in testing this every time.
	  dotest cvsadm-1a "cat CVS/Root" ${REP}
	  dotest cvsadm-1b "cat CVS/Repository" "\."
	  dotest cvsadm-1c "cat CVSROOT/CVS/Root" ${REP}
	  dotest cvsadm-1d "cat CVSROOT/CVS/Repository" "CVSROOT"
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
${PROG} commit: Rebuilding administrative file database" \
"${PROG} commit: Examining .
${PROG} commit: Examining CVSROOT"
	  rm -rf CVS CVSROOT;

	  # Create the various modules
	  dotest cvsadm-2 "${testcvs} -q co -l ." ''
	  mkdir mod1
	  mkdir mod1-2
	  mkdir mod2
	  mkdir mod2/sub2
	  mkdir mod2-2
	  mkdir mod2-2/sub2-2
	  dotest cvsadm-2a "${testcvs} add mod1 mod1-2 mod2 mod2/sub2 mod2-2 mod2-2/sub2-2" \
"Directory ${CVSROOT_DIRNAME}/mod1 added to the repository
Directory ${CVSROOT_DIRNAME}/mod1-2 added to the repository
Directory ${CVSROOT_DIRNAME}/mod2 added to the repository
Directory ${CVSROOT_DIRNAME}/mod2/sub2 added to the repository
Directory ${CVSROOT_DIRNAME}/mod2-2 added to the repository
Directory ${CVSROOT_DIRNAME}/mod2-2/sub2-2 added to the repository"

	  # Populate the directories for the halibut
	  echo "file1" > mod1/file1
	  echo "file1-2" > mod1-2/file1-2
	  echo "file2" > mod2/sub2/file2
	  echo "file2-2" > mod2-2/sub2-2/file2-2
	  dotest cvsadm-2aa "${testcvs} add mod1/file1 mod1-2/file1-2 mod2/sub2/file2 mod2-2/sub2-2/file2-2" \
"${PROG} add: scheduling file .mod1/file1. for addition
${PROG} add: scheduling file .mod1-2/file1-2. for addition
${PROG} add: scheduling file .mod2/sub2/file2. for addition
${PROG} add: scheduling file .mod2-2/sub2-2/file2-2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"

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
"${PROG} checkout: Updating 1mod
U 1mod/file1"
	  dotest cvsadm-3b "cat CVS/Repository" "\."
	  dotest cvsadm-3d "cat 1mod/CVS/Repository" "mod1"
	  rm -rf CVS 1mod

	  dotest cvsadm-4 "${testcvs} co 2mod" \
"${PROG} checkout: Updating 2mod
U 2mod/file2"
	  dotest cvsadm-4b "cat CVS/Repository" "\."
	  dotest cvsadm-4d "cat 2mod/CVS/Repository" "mod2/sub2"
	  rm -rf CVS 2mod

	  dotest cvsadm-5 "${testcvs} co 1d1mod" \
"${PROG} checkout: Updating dir1d1
U dir1d1/file1"
	  dotest cvsadm-5b "cat CVS/Repository" "\."
	  dotest cvsadm-5d "cat dir1d1/CVS/Repository" "mod1"
	  rm -rf CVS dir1d1

	  dotest cvsadm-6 "${testcvs} co 1d2mod" \
"${PROG} checkout: Updating dir1d2
U dir1d2/file2"
	  dotest cvsadm-6b "cat CVS/Repository" "\."
	  dotest cvsadm-6d "cat dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir1d2

	  dotest cvsadm-7 "${testcvs} co 2d1mod" \
"${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  dotest cvsadm-7b "cat CVS/Repository" "\."
	  dotest cvsadm-7d "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-7f "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir2d1

	  dotest cvsadm-8 "${testcvs} co 2d2mod" \
"${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  dotest cvsadm-8b "cat CVS/Repository" "\."
	  dotest cvsadm-8d "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-8f "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir2d2

	  ##################################################
	  ## You are in a shell script of twisted little
	  ## module combination statements, all alike.
	  ##################################################

	  ### 1mod
	  
	  dotest cvsadm-9 "${testcvs} co 1mod 1mod-2" \
"${PROG} checkout: Updating 1mod
U 1mod/file1
${PROG} checkout: Updating 1mod-2
U 1mod-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-9b "cat CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-9d "cat 1mod/CVS/Repository" "mod1"
	  # the usual for 1mod copy
	  dotest cvsadm-9f "cat 1mod-2/CVS/Repository" "mod1-2"
	  rm -rf CVS 1mod 1mod-2

	  # 1mod 2mod redmod bluemod
	  dotest cvsadm-10 "${testcvs} co 1mod 2mod" \
"${PROG} checkout: Updating 1mod
U 1mod/file1
${PROG} checkout: Updating 2mod
U 2mod/file2"
	  # the usual for the top level
	  dotest cvsadm-10b "cat CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-10d "cat 1mod/CVS/Repository" "mod1"
	  # the usual for 2dmod
	  dotest cvsadm-10f "cat 2mod/CVS/Repository" "mod2/sub2"
	  rm -rf CVS 1mod 2mod

	  dotest cvsadm-11 "${testcvs} co 1mod 1d1mod" \
"${PROG} checkout: Updating 1mod
U 1mod/file1
${PROG} checkout: Updating dir1d1
U dir1d1/file1"
	  # the usual for the top level
	  dotest cvsadm-11b "cat CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-11d "cat 1mod/CVS/Repository" "mod1"
	  # the usual for 1d1mod
	  dotest cvsadm-11f "cat dir1d1/CVS/Repository" "mod1"
	  rm -rf CVS 1mod dir1d1

	  dotest cvsadm-12 "${testcvs} co 1mod 1d2mod" \
"${PROG} checkout: Updating 1mod
U 1mod/file1
${PROG} checkout: Updating dir1d2
U dir1d2/file2"
	  # the usual for the top level
	  dotest cvsadm-12b "cat CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-12d "cat 1mod/CVS/Repository" "mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-12f "cat dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS 1mod dir1d2

	  dotest cvsadm-13 "${testcvs} co 1mod 2d1mod" \
"${PROG} checkout: Updating 1mod
U 1mod/file1
${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-13b "cat CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-13d "cat 1mod/CVS/Repository" "mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-13f "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-13h "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS 1mod dir2d1

	  dotest cvsadm-14 "${testcvs} co 1mod 2d2mod" \
"${PROG} checkout: Updating 1mod
U 1mod/file1
${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-14b "cat CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-14d "cat 1mod/CVS/Repository" "mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-14f "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-14h "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS 1mod dir2d2


	  ### 2mod
	  
	  dotest cvsadm-15 "${testcvs} co 2mod 2mod-2" \
"${PROG} checkout: Updating 2mod
U 2mod/file2
${PROG} checkout: Updating 2mod-2
U 2mod-2/file2-2"
	  # the usual for the top level
	  dotest cvsadm-15b "cat CVS/Repository" "\."
	  # the usual for 2mod
	  dotest cvsadm-15d "cat 2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 2mod copy
	  dotest cvsadm-15f "cat 2mod-2/CVS/Repository" "mod2-2/sub2-2"
	  rm -rf CVS 2mod 2mod-2


	  dotest cvsadm-16 "${testcvs} co 2mod 1d1mod" \
"${PROG} checkout: Updating 2mod
U 2mod/file2
${PROG} checkout: Updating dir1d1
U dir1d1/file1"
	  # the usual for the top level
	  dotest cvsadm-16b "cat CVS/Repository" "\."
	  # the usual for 2mod
	  dotest cvsadm-16d "cat 2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 1d1mod
	  dotest cvsadm-16f "cat dir1d1/CVS/Repository" "mod1"
	  rm -rf CVS 2mod dir1d1

	  dotest cvsadm-17 "${testcvs} co 2mod 1d2mod" \
"${PROG} checkout: Updating 2mod
U 2mod/file2
${PROG} checkout: Updating dir1d2
U dir1d2/file2"
	  # the usual for the top level
	  dotest cvsadm-17b "cat CVS/Repository" "\."
	  # the usual for 2mod
	  dotest cvsadm-17d "cat 2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 1d2mod
	  dotest cvsadm-17f "cat dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS 2mod dir1d2

	  dotest cvsadm-18 "${testcvs} co 2mod 2d1mod" \
"${PROG} checkout: Updating 2mod
U 2mod/file2
${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-18b "cat CVS/Repository" "\."
	  # the usual for 2mod
	  dotest cvsadm-18d "cat 2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-18f "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-18h "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS 2mod dir2d1

	  dotest cvsadm-19 "${testcvs} co 2mod 2d2mod" \
"${PROG} checkout: Updating 2mod
U 2mod/file2
${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-19b "cat CVS/Repository" "\."
	  # the usual for 2mod
	  dotest cvsadm-19d "cat 2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-19f "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-19h "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS 2mod dir2d2


	  ### 1d1mod

	  dotest cvsadm-20 "${testcvs} co 1d1mod 1d1mod-2" \
"${PROG} checkout: Updating dir1d1
U dir1d1/file1
${PROG} checkout: Updating dir1d1-2
U dir1d1-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-20b "cat CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-20d "cat dir1d1/CVS/Repository" "mod1"
	  # the usual for 1d1mod copy
	  dotest cvsadm-20f "cat dir1d1-2/CVS/Repository" "mod1-2"
	  rm -rf CVS dir1d1 dir1d1-2

	  dotest cvsadm-21 "${testcvs} co 1d1mod 1d2mod" \
"${PROG} checkout: Updating dir1d1
U dir1d1/file1
${PROG} checkout: Updating dir1d2
U dir1d2/file2"
	  # the usual for the top level
	  dotest cvsadm-21b "cat CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-21d "cat dir1d1/CVS/Repository" "mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-21f "cat dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir1d1 dir1d2

	  dotest cvsadm-22 "${testcvs} co 1d1mod 2d1mod" \
"${PROG} checkout: Updating dir1d1
U dir1d1/file1
${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-22b "cat CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-22d "cat dir1d1/CVS/Repository" "mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-22f "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-22h "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir1d1 dir2d1

	  dotest cvsadm-23 "${testcvs} co 1d1mod 2d2mod" \
"${PROG} checkout: Updating dir1d1
U dir1d1/file1
${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-23b "cat CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-23d "cat dir1d1/CVS/Repository" "mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-23f "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-23h "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir1d1 dir2d2


	  ### 1d2mod

	  dotest cvsadm-24 "${testcvs} co 1d2mod 1d2mod-2" \
"${PROG} checkout: Updating dir1d2
U dir1d2/file2
${PROG} checkout: Updating dir1d2-2
U dir1d2-2/file2-2"
	  # the usual for the top level
	  dotest cvsadm-24b "cat CVS/Repository" "\."
	  # the usual for 1d2mod
	  dotest cvsadm-24d "cat dir1d2/CVS/Repository" "mod2/sub2"
	  # the usual for 1d2mod copy
	  dotest cvsadm-24f "cat dir1d2-2/CVS/Repository" "mod2-2/sub2-2"
	  rm -rf CVS dir1d2 dir1d2-2

	  dotest cvsadm-25 "${testcvs} co 1d2mod 2d1mod" \
"${PROG} checkout: Updating dir1d2
U dir1d2/file2
${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	  # the usual for the top level
	  dotest cvsadm-25b "cat CVS/Repository" "\."
	  # the usual for 1d2mod
	  dotest cvsadm-25d "cat dir1d2/CVS/Repository" "mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-25f "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-25h "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir1d2 dir2d1

	  dotest cvsadm-26 "${testcvs} co 1d2mod 2d2mod" \
"${PROG} checkout: Updating dir1d2
U dir1d2/file2
${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-26b "cat CVS/Repository" "\."
	  # the usual for 1d2mod
	  dotest cvsadm-26d "cat dir1d2/CVS/Repository" "mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-26f "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-26h "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir1d2 dir2d2


	  # 2d1mod

	  dotest cvsadm-27 "${testcvs} co 2d1mod 2d1mod-2" \
"${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1
${PROG} checkout: Updating dir2d1-2/sub2d1-2
U dir2d1-2/sub2d1-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-27b "cat CVS/Repository" "\."
	  # the usual for 2d1mod
	  dotest cvsadm-27d "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-27f "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-27h "cat dir2d1-2/CVS/Repository" "\."
	  dotest cvsadm-27j "cat dir2d1-2/sub2d1-2/CVS/Repository" "mod1-2"
	  rm -rf CVS dir2d1 dir2d1-2

	  dotest cvsadm-28 "${testcvs} co 2d1mod 2d2mod" \
"${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1
${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	  # the usual for the top level
	  dotest cvsadm-28b "cat CVS/Repository" "\."
	  # the usual for 2d1mod
	  dotest cvsadm-28d "cat dir2d1/CVS/Repository" "\."
	  dotest cvsadm-28f "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-28h "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-28j "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir2d1 dir2d2

	  
	  # 2d2mod

	  dotest cvsadm-29 "${testcvs} co 2d2mod 2d2mod-2" \
"${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2
${PROG} checkout: Updating dir2d2-2/sub2d2-2
U dir2d2-2/sub2d2-2/file2-2"
	  # the usual for the top level
	  dotest cvsadm-29b "cat CVS/Repository" "\."
	  # the usual for 2d2mod
	  dotest cvsadm-29d "cat dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-29f "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-29h "cat dir2d2-2/CVS/Repository" "mod2-2"
	  dotest cvsadm-29j "cat dir2d2-2/sub2d2-2/CVS/Repository" \
"mod2-2/sub2-2"
	  rm -rf CVS dir2d2 dir2d2-2

	  ##################################################
	  ## And now, all of that again using the "-d" flag
	  ## on the command line.
	  ##################################################

	  dotest cvsadm-1d3 "${testcvs} co -d dir 1mod" \
"${PROG} checkout: Updating dir
U dir/file1"
	  dotest cvsadm-1d3b "cat CVS/Repository" "\."
	  dotest cvsadm-1d3d "cat dir/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d4 "${testcvs} co -d dir 2mod" \
"${PROG} checkout: Updating dir
U dir/file2"
	  dotest cvsadm-1d4b "cat CVS/Repository" "\."
	  dotest cvsadm-1d4d "cat dir/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d5 "${testcvs} co -d dir 1d1mod" \
"${PROG} checkout: Updating dir
U dir/file1"
	  dotest cvsadm-1d5b "cat CVS/Repository" "\."
	  dotest cvsadm-1d5d "cat dir/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d6 "${testcvs} co -d dir 1d2mod" \
"${PROG} checkout: Updating dir
U dir/file2"
	  dotest cvsadm-1d6b "cat CVS/Repository" "\."
	  dotest cvsadm-1d6d "cat dir/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d7 "${testcvs} co -d dir 2d1mod" \
"${PROG} checkout: Updating dir
U dir/file1"
	  dotest cvsadm-1d7b "cat CVS/Repository" "\."
	  dotest cvsadm-1d7d "cat dir/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d8 "${testcvs} co -d dir 2d2mod" \
"${PROG} checkout: Updating dir
U dir/file2"
	  dotest cvsadm-1d8b "cat CVS/Repository" "\."
	  dotest cvsadm-1d8d "cat dir/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  ##################################################
	  ## Los Combonaciones
	  ##################################################

	  ### 1mod

	  dotest cvsadm-1d9 "${testcvs} co -d dir 1mod 1mod-2" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1
${PROG} checkout: Updating dir/1mod-2
U dir/1mod-2/file1-2"
	  # the usual for the top level
	  dotest cvsadm-1d9b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d9d "cat dir/CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-1d9f "cat dir/1mod/CVS/Repository" "mod1"
	  # the usual for 1mod copy
	  dotest cvsadm-1d9h "cat dir/1mod-2/CVS/Repository" "mod1-2"
	  rm -rf CVS dir

	  # 1mod 2mod redmod bluemod
	  dotest cvsadm-1d10 "${testcvs} co -d dir 1mod 2mod" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1
${PROG} checkout: Updating dir/2mod
U dir/2mod/file2"
	  dotest cvsadm-1d10b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d10d "cat dir/CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-1d10f "cat dir/1mod/CVS/Repository" "mod1"
	  # the usual for 2dmod
	  dotest cvsadm-1d10h "cat dir/2mod/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d11 "${testcvs} co -d dir 1mod 1d1mod" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1
${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1"
	  dotest cvsadm-1d11b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d11d "cat dir/CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-1d11f "cat dir/1mod/CVS/Repository" "mod1"
	  # the usual for 1d1mod
	  dotest cvsadm-1d11h "cat dir/dir1d1/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d12 "${testcvs} co -d dir 1mod 1d2mod" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1
${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-1d12b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d12d "cat dir/CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-1d12f "cat dir/1mod/CVS/Repository" "mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-1d12h "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d13 "${testcvs} co -d dir 1mod 2d1mod" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1
${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d13b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d13d "cat dir/CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-1d13f "cat dir/1mod/CVS/Repository" "mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-1d13h "cat dir/dir2d1/CVS/Repository" "\."
	  dotest cvsadm-1d13j "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d14 "${testcvs} co -d dir 1mod 2d2mod" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1
${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d14b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d14d "cat dir/CVS/Repository" "\."
	  # the usual for 1mod
	  dotest cvsadm-1d14f "cat dir/1mod/CVS/Repository" "mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-1d14h "cat dir/dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-1d14j "cat dir/dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir


	  ### 2mod

	  dotest cvsadm-1d15 "${testcvs} co -d dir 2mod 2mod-2" \
"${PROG} checkout: Updating dir/2mod
U dir/2mod/file2
${PROG} checkout: Updating dir/2mod-2
U dir/2mod-2/file2-2"
	  dotest cvsadm-1d15b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d15d "cat dir/CVS/Repository" "mod2"
	  # the usual for 2mod
	  dotest cvsadm-1d15f "cat dir/2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 2mod copy
	  dotest cvsadm-1d15h "cat dir/2mod-2/CVS/Repository" "mod2-2/sub2-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d16 "${testcvs} co -d dir 2mod 1d1mod" \
"${PROG} checkout: Updating dir/2mod
U dir/2mod/file2
${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1"
	  dotest cvsadm-1d16b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d16d "cat dir/CVS/Repository" "mod2"
	  # the usual for 2mod
	  dotest cvsadm-1d16f "cat dir/2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 1d1mod
	  dotest cvsadm-1d16h "cat dir/dir1d1/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d17 "${testcvs} co -d dir 2mod 1d2mod" \
"${PROG} checkout: Updating dir/2mod
U dir/2mod/file2
${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-1d17b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d17d "cat dir/CVS/Repository" "mod2"
	  # the usual for 2mod
	  dotest cvsadm-1d17f "cat dir/2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 1d2mod
	  dotest cvsadm-1d17h "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d18 "${testcvs} co -d dir 2mod 2d1mod" \
"${PROG} checkout: Updating dir/2mod
U dir/2mod/file2
${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d18b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d18d "cat dir/CVS/Repository" "mod2"
	  # the usual for 2mod
	  dotest cvsadm-1d18f "cat dir/2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-1d18h "cat dir/dir2d1/CVS/Repository" "\."
	  dotest cvsadm-1d18j "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d19 "${testcvs} co -d dir 2mod 2d2mod" \
"${PROG} checkout: Updating dir/2mod
U dir/2mod/file2
${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d19b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d19d "cat dir/CVS/Repository" "mod2"
	  # the usual for 2mod
	  dotest cvsadm-1d19f "cat dir/2mod/CVS/Repository" "mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-1d19h "cat dir/dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-1d19j "cat dir/dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir


	  ### 1d1mod

	  dotest cvsadm-1d20 "${testcvs} co -d dir 1d1mod 1d1mod-2" \
"${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} checkout: Updating dir/dir1d1-2
U dir/dir1d1-2/file1-2"
	  dotest cvsadm-1d20b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d20d "cat dir/CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d20f "cat dir/dir1d1/CVS/Repository" "mod1"
	  # the usual for 1d1mod copy
	  dotest cvsadm-1d20h "cat dir/dir1d1-2/CVS/Repository" "mod1-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d21 "${testcvs} co -d dir 1d1mod 1d2mod" \
"${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2"
	  dotest cvsadm-1d21b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d21d "cat dir/CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d21f "cat dir/dir1d1/CVS/Repository" "mod1"
	  # the usual for 1d2mod
	  dotest cvsadm-1d21h "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  dotest cvsadm-1d22 "${testcvs} co -d dir 1d1mod 2d1mod" \
"${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d22b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d22d "cat dir/CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d22f "cat dir/dir1d1/CVS/Repository" "mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-1d22h "cat dir/dir2d1/CVS/Repository" "\."
	  dotest cvsadm-1d22j "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d23 "${testcvs} co -d dir 1d1mod 2d2mod" \
"${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1
${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d23b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d23d "cat dir/CVS/Repository" "\."
	  # the usual for 1d1mod
	  dotest cvsadm-1d23f "cat dir/dir1d1/CVS/Repository" "mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-1d23h "cat dir/dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-1d23j "cat dir/dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir


	  ### 1d2mod

	  dotest cvsadm-1d24 "${testcvs} co -d dir 1d2mod 1d2mod-2" \
"${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2
${PROG} checkout: Updating dir/dir1d2-2
U dir/dir1d2-2/file2-2"
	  dotest cvsadm-1d24b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d24d "cat dir/CVS/Repository" "mod2"
	  # the usual for 1d2mod
	  dotest cvsadm-1d24f "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	  # the usual for 1d2mod copy
	  dotest cvsadm-1d24h "cat dir/dir1d2-2/CVS/Repository" "mod2-2/sub2-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d25 "${testcvs} co -d dir 1d2mod 2d1mod" \
"${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2
${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	  dotest cvsadm-1d25b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d25d "cat dir/CVS/Repository" "mod2"
	  # the usual for 1d2mod
	  dotest cvsadm-1d25f "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	  # the usual for 2d1mod
	  dotest cvsadm-1d25h "cat dir/dir2d1/CVS/Repository" "\."
	  dotest cvsadm-1d25j "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	  rm -rf CVS dir

	  dotest cvsadm-1d26 "${testcvs} co -d dir 1d2mod 2d2mod" \
"${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2
${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d26b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d26d "cat dir/CVS/Repository" "mod2"
	  # the usual for 1d2mod
	  dotest cvsadm-1d26f "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-1d26h "cat dir/dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-1d26j "cat dir/dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir


	  # 2d1mod

	  dotest cvsadm-1d27 "${testcvs} co -d dir 2d1mod 2d1mod-2" \
"${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1
${PROG} checkout: Updating dir/dir2d1-2/sub2d1-2
U dir/dir2d1-2/sub2d1-2/file1-2"
	  dotest cvsadm-1d27b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d27d "cat dir/CVS/Repository" "CVSROOT/Emptydir"
	  # the usual for 2d1mod
	  dotest cvsadm-1d27f "cat dir/dir2d1/CVS/Repository" "\."
	  dotest cvsadm-1d27h "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	  # the usual for 2d1mod
	  dotest cvsadm-1d27j "cat dir/dir2d1-2/CVS/Repository" "\."
	  dotest cvsadm-1d27l "cat dir/dir2d1-2/sub2d1-2/CVS/Repository" \
"mod1-2"
	  rm -rf CVS dir

	  dotest cvsadm-1d28 "${testcvs} co -d dir 2d1mod 2d2mod" \
"${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1
${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	  dotest cvsadm-1d28b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d28d "cat dir/CVS/Repository" "CVSROOT/Emptydir"
	  # the usual for 2d1mod
	  dotest cvsadm-1d28f "cat dir/dir2d1/CVS/Repository" "\."
	  dotest cvsadm-1d28h "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	  # the usual for 2d2mod
	  dotest cvsadm-1d28j "cat dir/dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-1d28l "cat dir/dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  rm -rf CVS dir

	  
	  # 2d2mod

	  dotest cvsadm-1d29 "${testcvs} co -d dir 2d2mod 2d2mod-2" \
"${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2
${PROG} checkout: Updating dir/dir2d2-2/sub2d2-2
U dir/dir2d2-2/sub2d2-2/file2-2"
	  dotest cvsadm-1d29b "cat CVS/Repository" "\."
	  # the usual for the dir level
	  dotest cvsadm-1d29d "cat dir/CVS/Repository" "\."
	  # the usual for 2d2mod
	  dotest cvsadm-1d29f "cat dir/dir2d2/CVS/Repository" "mod2"
	  dotest cvsadm-1d29h "cat dir/dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	  # the usual for 2d2mod
	  dotest cvsadm-1d29j "cat dir/dir2d2-2/CVS/Repository" "mod2-2"
	  dotest cvsadm-1d29l "cat dir/dir2d2-2/sub2d2-2/CVS/Repository" \
"mod2-2/sub2-2"
	  rm -rf CVS dir

	  ##################################################
	  ## And now, some of that again using the "-d" flag
	  ## on the command line, but use a longer path.
	  ##################################################

	  dotest_fail cvsadm-2d3-1 "${testcvs} co -d dir/dir2 1mod" \
"${PROG} \[checkout aborted\]: could not change directory to requested checkout directory .dir.: No such file or directory"

	  if $remote; then :; else
	    # Remote can't handle this, even with the "mkdir dir".
	    # This was also true of CVS 1.9.

	    mkdir dir
	    dotest cvsadm-2d3 "${testcvs} co -d dir/dir2 1mod" \
"${PROG} checkout: Updating dir/dir2
U dir/dir2/file1"
	    dotest cvsadm-2d3b "cat CVS/Repository" "\."
	    dotest_fail cvsadm-2d3d "test -f dir/CVS/Repository" ""
	    dotest cvsadm-2d3f "cat dir/dir2/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-2d4 "${testcvs} co -d dir/dir2 2mod" \
"${PROG} checkout: Updating dir/dir2
U dir/dir2/file2"
	    dotest cvsadm-2d4b "cat CVS/Repository" "\."
	    dotest cvsadm-2d4f "cat dir/dir2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-2d5 "${testcvs} co -d dir/dir2 1d1mod" \
"${PROG} checkout: Updating dir/dir2
U dir/dir2/file1"
	    dotest cvsadm-2d5b "cat CVS/Repository" "\."
	    dotest cvsadm-2d5f "cat dir/dir2/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-2d6 "${testcvs} co -d dir/dir2 1d2mod" \
"${PROG} checkout: Updating dir/dir2
U dir/dir2/file2"
	    dotest cvsadm-2d6b "cat CVS/Repository" "\."
	    dotest cvsadm-2d6f "cat dir/dir2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-2d7 "${testcvs} co -d dir/dir2 2d1mod" \
"${PROG} checkout: Updating dir/dir2
U dir/dir2/file1"
	    dotest cvsadm-2d7b "cat CVS/Repository" "\."
	    dotest cvsadm-2d7f "cat dir/dir2/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-2d8 "${testcvs} co -d dir/dir2 2d2mod" \
"${PROG} checkout: Updating dir/dir2
U dir/dir2/file2"
	    dotest cvsadm-2d8b "cat CVS/Repository" "\."
	    dotest cvsadm-2d8f "cat dir/dir2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    ##################################################
	    ## And now, a few of those tests revisited to
	    ## test the behavior of the -N flag.
	    ##################################################

	    dotest cvsadm-N3 "${testcvs} co -N 1mod" \
"${PROG} checkout: Updating 1mod
U 1mod/file1"
	    dotest cvsadm-N3b "cat CVS/Repository" "\."
	    dotest cvsadm-N3d "cat 1mod/CVS/Repository" "mod1"
	    rm -rf CVS 1mod

	    dotest cvsadm-N4 "${testcvs} co -N 2mod" \
"${PROG} checkout: Updating 2mod
U 2mod/file2"
	    dotest cvsadm-N4b "cat CVS/Repository" "\."
	    dotest cvsadm-N4d "cat 2mod/CVS/Repository" "mod2/sub2"
	    rm -rf CVS 2mod

	    dotest cvsadm-N5 "${testcvs} co -N 1d1mod" \
"${PROG} checkout: Updating dir1d1
U dir1d1/file1"
	    dotest cvsadm-N5b "cat CVS/Repository" "\."
	    dotest cvsadm-N5d "cat dir1d1/CVS/Repository" "mod1"
	    rm -rf CVS dir1d1

	    dotest cvsadm-N6 "${testcvs} co -N 1d2mod" \
"${PROG} checkout: Updating dir1d2
U dir1d2/file2"
	    dotest cvsadm-N6b "cat CVS/Repository" "\."
	    dotest cvsadm-N6d "cat dir1d2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir1d2

	    dotest cvsadm-N7 "${testcvs} co -N 2d1mod" \
"${PROG} checkout: Updating dir2d1/sub2d1
U dir2d1/sub2d1/file1"
	    dotest cvsadm-N7b "cat CVS/Repository" "\."
	    dotest cvsadm-N7d "cat dir2d1/CVS/Repository" "\."
	    dotest cvsadm-N7f "cat dir2d1/sub2d1/CVS/Repository" "mod1"
	    rm -rf CVS dir2d1

	    dotest cvsadm-N8 "${testcvs} co -N 2d2mod" \
"${PROG} checkout: Updating dir2d2/sub2d2
U dir2d2/sub2d2/file2"
	    dotest cvsadm-N8b "cat CVS/Repository" "\."
	    dotest cvsadm-N8d "cat dir2d2/CVS/Repository" "mod2"
	    dotest cvsadm-N8f "cat dir2d2/sub2d2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir2d2

	    ## the ones in one-deep directories

	    dotest cvsadm-N1d3 "${testcvs} co -N -d dir 1mod" \
"${PROG} checkout: Updating dir/1mod
U dir/1mod/file1"
	    dotest cvsadm-N1d3b "cat CVS/Repository" "\."
	    dotest cvsadm-N1d3d "cat dir/CVS/Repository" "\."
	    dotest cvsadm-N1d3f "cat dir/1mod/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    dotest cvsadm-N1d4 "${testcvs} co -N -d dir 2mod" \
"${PROG} checkout: Updating dir/2mod
U dir/2mod/file2"
	    dotest cvsadm-N1d4b "cat CVS/Repository" "\."
	    dotest cvsadm-N1d4d "cat dir/CVS/Repository" "mod2"
	    dotest cvsadm-N1d4f "cat dir/2mod/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    dotest cvsadm-N1d5 "${testcvs} co -N -d dir 1d1mod" \
"${PROG} checkout: Updating dir/dir1d1
U dir/dir1d1/file1"
	    dotest cvsadm-N1d5b "cat CVS/Repository" "\."
	    dotest cvsadm-N1d5d "cat dir/CVS/Repository" "\."
	    dotest cvsadm-N1d5d "cat dir/dir1d1/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    dotest cvsadm-N1d6 "${testcvs} co -N -d dir 1d2mod" \
"${PROG} checkout: Updating dir/dir1d2
U dir/dir1d2/file2"
	    dotest cvsadm-N1d6b "cat CVS/Repository" "\."
	    dotest cvsadm-N1d6d "cat dir/CVS/Repository" "mod2"
	    dotest cvsadm-N1d6f "cat dir/dir1d2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    dotest cvsadm-N1d7 "${testcvs} co -N -d dir 2d1mod" \
"${PROG} checkout: Updating dir/dir2d1/sub2d1
U dir/dir2d1/sub2d1/file1"
	    dotest cvsadm-N1d7b "cat CVS/Repository" "\."
	    dotest cvsadm-N1d7d "cat dir/CVS/Repository" "CVSROOT/Emptydir"
	    dotest cvsadm-N1d7f "cat dir/dir2d1/CVS/Repository" "\."
	    dotest cvsadm-N1d7h "cat dir/dir2d1/sub2d1/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    dotest cvsadm-N1d8 "${testcvs} co -N -d dir 2d2mod" \
"${PROG} checkout: Updating dir/dir2d2/sub2d2
U dir/dir2d2/sub2d2/file2"
	    dotest cvsadm-N1d8b "cat CVS/Repository" "\."
	    dotest cvsadm-N1d8d "cat dir/CVS/Repository" "\."
	    dotest cvsadm-N1d8d "cat dir/dir2d2/CVS/Repository" "mod2"
	    dotest cvsadm-N1d8d "cat dir/dir2d2/sub2d2/CVS/Repository" \
"mod2/sub2"
	    rm -rf CVS dir

	    ## the ones in two-deep directories

	    mkdir dir
	    dotest cvsadm-N2d3 "${testcvs} co -N -d dir/dir2 1mod" \
"${PROG} checkout: Updating dir/dir2/1mod
U dir/dir2/1mod/file1"
	    dotest cvsadm-N2d3b "cat CVS/Repository" "\."
	    dotest cvsadm-N2d3f "cat dir/dir2/CVS/Repository" "\."
	    dotest cvsadm-N2d3h "cat dir/dir2/1mod/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-N2d4 "${testcvs} co -N -d dir/dir2 2mod" \
"${PROG} checkout: Updating dir/dir2/2mod
U dir/dir2/2mod/file2"
	    dotest cvsadm-N2d4b "cat CVS/Repository" "\."
	    dotest cvsadm-N2d4f "cat dir/dir2/CVS/Repository" "mod2"
	    dotest cvsadm-N2d4h "cat dir/dir2/2mod/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-N2d5 "${testcvs} co -N -d dir/dir2 1d1mod" \
"${PROG} checkout: Updating dir/dir2/dir1d1
U dir/dir2/dir1d1/file1"
	    dotest cvsadm-N2d5b "cat CVS/Repository" "\."
	    dotest cvsadm-N2d5f "cat dir/dir2/CVS/Repository" "\."
	    dotest cvsadm-N2d5h "cat dir/dir2/dir1d1/CVS/Repository" "mod1"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-N2d6 "${testcvs} co -N -d dir/dir2 1d2mod" \
"${PROG} checkout: Updating dir/dir2/dir1d2
U dir/dir2/dir1d2/file2"
	    dotest cvsadm-N2d6b "cat CVS/Repository" "\."
	    dotest cvsadm-N2d6f "cat dir/dir2/CVS/Repository" "mod2"
	    dotest cvsadm-N2d6h "cat dir/dir2/dir1d2/CVS/Repository" "mod2/sub2"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-N2d7 "${testcvs} co -N -d dir/dir2 2d1mod" \
"${PROG} checkout: Updating dir/dir2/dir2d1/sub2d1
U dir/dir2/dir2d1/sub2d1/file1"
	    dotest cvsadm-N2d7b "cat CVS/Repository" "\."
	    dotest cvsadm-N2d7f "cat dir/dir2/CVS/Repository" "CVSROOT/Emptydir"
	    dotest cvsadm-N2d7g "cat dir/dir2/dir2d1/CVS/Repository" "\."
	    dotest cvsadm-N2d7h "cat dir/dir2/dir2d1/sub2d1/CVS/Repository" \
"mod1"
	    rm -rf CVS dir

	    mkdir dir
	    dotest cvsadm-N2d8 "${testcvs} co -N -d dir/dir2 2d2mod" \
"${PROG} checkout: Updating dir/dir2/dir2d2/sub2d2
U dir/dir2/dir2d2/sub2d2/file2"
	    dotest cvsadm-N2d8b "cat CVS/Repository" "\."
	    dotest cvsadm-N2d8f "cat dir/dir2/CVS/Repository" "\."
	    dotest cvsadm-N2d8h "cat dir/dir2/dir2d2/CVS/Repository" "mod2"
	    dotest cvsadm-N2d8j "cat dir/dir2/dir2d2/sub2d2/CVS/Repository" \
"mod2/sub2"
	    rm -rf CVS dir

	  fi # end of tests to be skipped for remote

	  ##################################################
	  ## That's enough of that, thank you very much.
	  ##################################################

	  dotest cvsadm-cleanup-1 "${testcvs} -q co CVSROOT/config" \
"U CVSROOT/config"
	  cd CVSROOT
	  echo "# empty file" >config
	  dotest cvsadm-cleanup-2 "${testcvs} -q ci -m cvsadm-cleanup" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
          cd ..
          rm -rf CVSROOT CVS

	  # remove our junk
	  cd ..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/1mod
	  rm -rf ${CVSROOT_DIRNAME}/1mod-2
	  rm -rf ${CVSROOT_DIRNAME}/2mod
	  rm -rf ${CVSROOT_DIRNAME}/2mod-2
	  rm -rf ${CVSROOT_DIRNAME}/mod1
	  rm -rf ${CVSROOT_DIRNAME}/mod1-2
	  rm -rf ${CVSROOT_DIRNAME}/mod2
	  rm -rf ${CVSROOT_DIRNAME}/mod2-2
	  ;;

	emptydir)
	  # Various tests of the Emptydir (CVSNULLREPOS) code.  See also:
	  #   cvsadm: tests of Emptydir in various module definitions
	  #   basicb: Test that "Emptydir" is non-special in ordinary contexts

	  mkdir 1; cd 1
	  dotest emptydir-1 "${testcvs} co CVSROOT/modules" \
"U CVSROOT/modules"
	  echo "# Module defs for emptydir tests" > CVSROOT/modules
	  echo "2d1mod -d dir2d1/sub/sub2d1 mod1" >> CVSROOT/modules
	  echo "2d1moda -d dir2d1/suba moda/modasub" >> CVSROOT/modules
	  echo "2d1modb -d dir2d1/suba mod1" >> CVSROOT/modules
	  echo "comb -a 2d1modb 2d1moda" >> CVSROOT/modules

	  dotest emptydir-2 "${testcvs} ci -m add-modules" \
"${PROG} [a-z]*: Examining CVSROOT
Checking in CVSROOT/modules;
${CVSROOT_DIRNAME}/CVSROOT/modules,v  <--  modules
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database" \
"${PROG} commit: Examining CVSROOT"
	  rm -rf CVS CVSROOT

	  mkdir ${CVSROOT_DIRNAME}/mod1 ${CVSROOT_DIRNAME}/moda
	  # Populate.  Not sure we really need to do this.
	  dotest emptydir-3 "${testcvs} -q co -l ." ""
	  dotest emptydir-3a "${testcvs} co mod1 moda" \
"${PROG} checkout: Updating mod1
${PROG} checkout: Updating moda"
	  echo "file1" > mod1/file1
	  mkdir moda/modasub
	  dotest emptydir-3b "${testcvs} add moda/modasub" \
"Directory ${CVSROOT_DIRNAME}/moda/modasub added to the repository"
	  echo "filea" > moda/modasub/filea
	  dotest emptydir-4 "${testcvs} add mod1/file1 moda/modasub/filea" \
"${PROG} add: scheduling file .mod1/file1. for addition
${PROG} add: scheduling file .moda/modasub/filea. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest emptydir-5 "${testcvs} -q ci -m yup" \
"RCS file: ${CVSROOT_DIRNAME}/mod1/file1,v
done
Checking in mod1/file1;
${CVSROOT_DIRNAME}/mod1/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/moda/modasub/filea,v
done
Checking in moda/modasub/filea;
${CVSROOT_DIRNAME}/moda/modasub/filea,v  <--  filea
initial revision: 1\.1
done"
	  rm -rf mod1 moda CVS
	  # End Populate.

	  dotest emptydir-6 "${testcvs} co 2d1mod" \
"${PROG} checkout: Updating dir2d1/sub/sub2d1
U dir2d1/sub/sub2d1/file1"
	  cd dir2d1
	  touch emptyfile
	  # It doesn't make any sense to add a file (or do much of anything
	  # else) in Emptydir; Emptydir is a placeholder indicating that
	  # the working directory doesn't correspond to anything in
	  # the repository.
	  dotest_fail emptydir-7 "${testcvs} add emptyfile" \
"${PROG} \[add aborted\]: cannot add to ${CVSROOT_DIRNAME}/CVSROOT/Emptydir"
	  mkdir emptydir
	  dotest_fail emptydir-8 "${testcvs} add emptydir" \
"${PROG} \[[a-z]* aborted\]: cannot add to ${CVSROOT_DIRNAME}/CVSROOT/Emptydir"
	  cd ..
	  rm -rf CVS dir2d1

	  # OK, while we have an Emptydir around, test a few obscure
	  # things about it.
	  mkdir edir; cd edir
	  dotest emptydir-9 "${testcvs} -q co -l CVSROOT" \
"U CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  dotest_fail emptydir-10 "test -d Emptydir" ''
	  # This tests the code in find_dirs which skips Emptydir.
	  dotest emptydir-11 "${testcvs} -q -n update -d -P" ''
	  cd ../..
	  rm -r edir
	  cd ..

	  # Now start playing with moda.
	  mkdir 2; cd 2
	  dotest emptydir-12 "${testcvs} -q co 2d1moda" \
"U dir2d1/suba/filea"
	  # OK, this is the crux of the matter.  This used to show "Emptydir",
	  # but everyone seemed to think it should show "moda".  This
	  # usually works better, but not always as shown by the following
	  # test.
	  dotest emptydir-13 "cat dir2d1/CVS/Repository" "moda"
	  dotest_fail emptydir-14 "${testcvs} co comb" \
"${PROG} checkout: existing repository ${CVSROOT_DIRNAME}/moda/modasub does not match ${CVSROOT_DIRNAME}/mod1
${PROG} checkout: ignoring module 2d1modb
${PROG} checkout: Updating dir2d1/suba"
	  dotest emptydir-15 "cat dir2d1/CVS/Repository" "moda"
	  cd ..

	  # Test the effect of a non-cvs directory already existing with the
	  # same name as one in the modules file.
	  mkdir 3; cd 3
	  mkdir dir2d1
	  dotest emptydir-16 "${testcvs} co 2d1mod" \
"${PROG} checkout: Updating dir2d1/sub/sub2d1
U dir2d1/sub/sub2d1/file1"
	  dotest emptydir-17 "test -d dir2d1/CVS"

	  # clean up
	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  rm -r 1 2 3
	  rm -rf ${CVSROOT_DIRNAME}/mod1 ${CVSROOT_DIRNAME}/moda
	  # I guess for the moment the convention is going to be
	  # that we don't need to remove ${CVSROOT_DIRNAME}/CVSROOT/Emptydir
	  ;;

	abspath)
	
	  # These tests test the thituations thin thwitch thoo theck
	  # things thout twith thabsolute thaths.  Threally.

	  #
	  # CHECKOUTS
	  #

	  # Create a few modules to use
	  mkdir ${CVSROOT_DIRNAME}/mod1 ${CVSROOT_DIRNAME}/mod2
	  dotest abspath-1a "${testcvs} co mod1 mod2" \
"${PROG} checkout: Updating mod1
${PROG} checkout: Updating mod2"

	  # Populate the module
	  echo "file1" > mod1/file1
	  echo "file2" > mod2/file2
	  cd mod1
	  dotest abspath-1ba "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
          cd ..
          cd mod2
	  dotest abspath-1bb "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
          cd ..

	  dotest abspath-1c "${testcvs} ci -m yup mod1 mod2" \
"${PROG} [a-z]*: Examining mod1
${PROG} [a-z]*: Examining mod2
RCS file: ${CVSROOT_DIRNAME}/mod1/file1,v
done
Checking in mod1/file1;
${CVSROOT_DIRNAME}/mod1/file1,v  <--  file1
initial revision: 1.1
done
RCS file: ${CVSROOT_DIRNAME}/mod2/file2,v
done
Checking in mod2/file2;
${CVSROOT_DIRNAME}/mod2/file2,v  <--  file2
initial revision: 1.1
done"
	  # Finished creating the module -- clean up.
	  rm -rf CVS mod1 mod2
	  # Done.
	  
	  # Try checking out the module in a local directory
	  if $remote; then
	    dotest_fail abspath-2a "${testcvs} co -d ${TESTDIR}/1 mod1" \
"${PROG} \[checkout aborted\]: absolute pathname .${TESTDIR}/1. illegal for server"
	    dotest abspath-2a-try2 "${testcvs} co -d 1 mod1" \
"${PROG} checkout: Updating 1
U 1/file1"
	  else
	    dotest abspath-2a "${testcvs} co -d ${TESTDIR}/1 mod1" \
"${PROG} checkout: Updating ${TESTDIR}/1
U ${TESTDIR}/1/file1"
	  fi # remote workaround

	  dotest abspath-2b "cat ${TESTDIR}/1/CVS/Repository" "mod1"

	  # Done.  Clean up.
	  rm -rf ${TESTDIR}/1


	  # Now try in a subdirectory.  We're not covering any more
	  # code here, but we might catch a future error if someone
	  # changes the checkout code.

	  # Note that for the same reason that the shell command
	  # "touch 1/2/3" requires directories 1 and 1/2 to already
	  # exist, we expect ${TESTDIR}/1 to already exist.  I believe
	  # this is the behavior of CVS 1.9 and earlier.
	  if $remote; then :; else
	    dotest_fail abspath-3.1 "${testcvs} co -d ${TESTDIR}/1/2 mod1" \
"${PROG} \[checkout aborted\]: could not change directory to requested checkout directory .${TESTDIR}/1.: No such file or directory"
	  fi
	  dotest_fail abspath-3.2 "${testcvs} co -d 1/2 mod1" \
"${PROG} \[checkout aborted\]: could not change directory to requested checkout directory .1.: No such file or directory"

	  mkdir 1

	  if $remote; then
	    # The server wants the directory to exist, but that is
	    # a bug, it should only need to exist on the client side.
	    # See also cvsadm-2d3.
	    dotest_fail abspath-3a "${testcvs} co -d 1/2 mod1" \
"${PROG} \[checkout aborted\]: could not change directory to requested checkout directory .1.: No such file or directory"
	    cd 1
	    dotest abspath-3a-try2 "${testcvs} co -d 2 mod1" \
"${PROG} checkout: Updating 2
U 2/file1"
	    cd ..
	    rm -rf 1/CVS
	  else
	  dotest abspath-3a "${testcvs} co -d ${TESTDIR}/1/2 mod1" \
"${PROG} checkout: Updating ${TESTDIR}/1/2
U ${TESTDIR}/1/2/file1"
	  fi # remote workaround
	  dotest abspath-3b "cat ${TESTDIR}/1/2/CVS/Repository" "mod1"

	  # For all the same reasons that we want "1" to already
	  # exist, we don't to mess with it to traverse it, for
	  # example by creating a CVS directory.

	  dotest_fail abspath-3c "test -d ${TESTDIR}/1/CVS" ''
	  # Done.  Clean up.
	  rm -rf ${TESTDIR}/1


	  # Now try someplace where we don't have permission.
	  mkdir ${TESTDIR}/barf
	  chmod -w ${TESTDIR}/barf
	    dotest_fail abspath-4r "${testcvs} co -d ${TESTDIR}/barf/sub mod1" \
"${PROG} \[checkout aborted\]: absolute pathname .${TESTDIR}/barf/sub. illegal for server" \
"${PROG} \[checkout aborted\]: cannot make directory sub: Permission denied"
	  chmod +w ${TESTDIR}/barf
	  rmdir ${TESTDIR}/barf
	  # Done.  Nothing to clean up.


	  # Try checking out two modules into the same directory.
	  if $remote; then
	    dotest abspath-5ar "${testcvs} co -d 1 mod1 mod2" \
"${PROG} checkout: Updating 1/mod1
U 1/mod1/file1
${PROG} checkout: Updating 1/mod2
U 1/mod2/file2"
	  else
	    dotest abspath-5a "${testcvs} co -d ${TESTDIR}/1 mod1 mod2" \
"${PROG} checkout: Updating ${TESTDIR}/1/mod1
U ${TESTDIR}/1/mod1/file1
${PROG} checkout: Updating ${TESTDIR}/1/mod2
U ${TESTDIR}/1/mod2/file2"
	  fi # end remote workaround
	  dotest abspath-5b "cat ${TESTDIR}/1/CVS/Repository" "\."
	  dotest abspath-5c "cat ${TESTDIR}/1/mod1/CVS/Repository" "mod1"
	  dotest abspath-5d "cat ${TESTDIR}/1/mod2/CVS/Repository" "mod2"
	  # Done.  Clean up.
	  rm -rf ${TESTDIR}/1


	  # Try checking out the top-level module.
	  if $remote; then
	    dotest abspath-6ar "${testcvs} co -d 1 ." \
"${PROG} checkout: Updating 1
${PROG} checkout: Updating 1/CVSROOT
${DOTSTAR}
${PROG} checkout: Updating 1/mod1
U 1/mod1/file1
${PROG} checkout: Updating 1/mod2
U 1/mod2/file2"
	  else
	    dotest abspath-6a "${testcvs} co -d ${TESTDIR}/1 ." \
"${PROG} checkout: Updating ${TESTDIR}/1
${PROG} checkout: Updating ${TESTDIR}/1/CVSROOT
${DOTSTAR}
${PROG} checkout: Updating ${TESTDIR}/1/mod1
U ${TESTDIR}/1/mod1/file1
${PROG} checkout: Updating ${TESTDIR}/1/mod2
U ${TESTDIR}/1/mod2/file2"
	  fi # end of remote workaround
	  dotest abspath-6b "cat ${TESTDIR}/1/CVS/Repository" "\."
	  dotest abspath-6c "cat ${TESTDIR}/1/CVSROOT/CVS/Repository" "CVSROOT"
	  dotest abspath-6c "cat ${TESTDIR}/1/mod1/CVS/Repository" "mod1"
	  dotest abspath-6d "cat ${TESTDIR}/1/mod2/CVS/Repository" "mod2"
	  # Done.  Clean up.
	  rm -rf ${TESTDIR}/1

	  # Test that an absolute pathname to some other directory
	  # doesn't mess with the current working directory.
	  mkdir 1
	  cd 1
	  if $remote; then
	    dotest_fail abspath-7ar "${testcvs} -q co -d ../2 mod2" \
"${PROG} checkout: protocol error: .\.\./2. contains more leading \.\.
${PROG} \[checkout aborted\]: than the 0 which Max-dotdot specified"
	    cd ..
	    dotest abspath-7a-try2r "${testcvs} -q co -d 2 mod2" \
"U 2/file2"
	    cd 1
	  else
	    dotest abspath-7a "${testcvs} -q co -d ${TESTDIR}/2 mod2" \
"U ${TESTDIR}/2/file2"
	  fi # remote workaround
	  dotest abspath-7b "ls" ""
	  dotest abspath-7c "${testcvs} -q co mod1" \
"U mod1/file1"
	  cd mod1
	  if $remote; then
	    cd ../..
	    dotest abspath-7dr "${testcvs} -q co -d 3 mod2" \
"U 3/file2"
	    cd 1/mod1
	  else
	  dotest abspath-7d "${testcvs} -q co -d ${TESTDIR}/3 mod2" \
"U ${TESTDIR}/3/file2"
	  fi # remote workaround
	  dotest abspath-7e "${testcvs} -q update -d" ""
	  cd ../..
	  rm -r 1 2 3

	  #
	  # FIXME: do other functions here (e.g. update /tmp/foo)
	  #

	  # Finished with all tests.  Remove the module.
	  rm -rf ${CVSROOT_DIRNAME}/mod1 ${CVSROOT_DIRNAME}/mod2

	  ;;



	abspath2)
	  # More absolute path checks.  The following used to attempt to create
	  # directories in /:
	  #
	  # $ cvs -d:fork:/cvsroot co /foo
	  # cvs checkout: warning: cannot make directory CVS in /: Permission denied
	  # cvs [checkout aborted]: cannot make directory /foo: Permission denied
	  # $
	  #
	  # The -z9 in this test also checks for an old server bug where the
	  # server would block indefinitely attempting to read an EOF from the
	  # client in the compression buffer shutdown routine.
	  dotest_fail abspath2-1 "$testcvs -z9 co /foo" \
"$PROG \[checkout aborted\]: Absolute module reference invalid: \`/foo'" \
"$PROG \[server aborted\]: Absolute module reference invalid: \`/foo'
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"
	  ;;



	toplevel)
	  # test the feature that cvs creates a CVS subdir also for
	  # the toplevel directory

	  # First set the TopLevelAdmin setting.
	  mkdir 1; cd 1
	  dotest toplevel-1a "${testcvs} -q co CVSROOT/config" \
"U CVSROOT/config"
	  cd CVSROOT
	  echo "TopLevelAdmin=yes" >config
	  dotest toplevel-1b "${testcvs} -q ci -m yes-top-level" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..
	  rm -r 1

	  mkdir 1; cd 1
	  dotest toplevel-1 "${testcvs} -q co -l ." ''
	  mkdir top-dir second-dir
	  dotest toplevel-2 "${testcvs} add top-dir second-dir" \
"Directory ${CVSROOT_DIRNAME}/top-dir added to the repository
Directory ${CVSROOT_DIRNAME}/second-dir added to the repository"
	  cd top-dir

	  touch file1
	  dotest toplevel-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest toplevel-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/top-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/top-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..

	  cd second-dir
	  touch file2
	  dotest toplevel-3s "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest toplevel-4s "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/second-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/second-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  cd ../..
	  rm -r 1; mkdir 1; cd 1
	  dotest toplevel-5 "${testcvs} co top-dir" \
"${PROG} checkout: Updating top-dir
U top-dir/file1"

	  dotest toplevel-6 "${testcvs} update top-dir" \
"${PROG} update: Updating top-dir"
	  dotest toplevel-7 "${testcvs} update"  \
"${PROG} update: Updating \.
${PROG} update: Updating top-dir"

	  dotest toplevel-8 "${testcvs} update -d top-dir" \
"${PROG} update: Updating top-dir"
	  # There is some sentiment that
	  #   "${PROG} update: Updating \.
          #   ${PROG} update: Updating top-dir"
	  # is correct but it isn't clear why that would be correct instead
	  # of the remote CVS behavior (which also updates CVSROOT).
	  #
	  # The DOTSTAR matches of a bunch of lines like
	  # "U CVSROOT/checkoutlist".  Trying to match them more precisely
	  # seemed to cause trouble.  For example CVSROOT/cvsignore will
	  # be present or absent depending on whether we ran the "ignore"
	  # test or not.
	  dotest toplevel-9 "${testcvs} update -d" \
"${PROG} update: Updating \.
${PROG} update: Updating CVSROOT
${DOTSTAR}
${PROG} update: Updating top-dir"

	  cd ..
	  rm -r 1; mkdir 1; cd 1
	  dotest toplevel-10 "${testcvs} co top-dir" \
"${PROG} checkout: Updating top-dir
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
	  if test -n "$remotehost"; then
	    # Cygwin again.
	    $CVS_RSH $remotehost "chmod -w $TESTDIR/1"
	  else
	    chmod -w ../1
	  fi
	  # Now see whether CVS has trouble because it can't create CVS.
	  # First string is for local, second is for remote.
	  dotest toplevel-12 "${testcvs} co top-dir" \
"${PROG} checkout: warning: cannot make directory CVS in \.: Permission denied
${PROG} checkout: Updating top-dir" \
"${PROG} checkout: warning: cannot make directory CVS in \.: Permission denied
${PROG} checkout: in directory \.:
${PROG} checkout: cannot open CVS/Entries for reading: No such file or directory
${PROG} checkout: Updating top-dir"

	  chmod +w ../1

	  dotest toplevel-cleanup-1 "${testcvs} -q co CVSROOT/config" \
"U CVSROOT/config"
	  cd CVSROOT
	  echo "# empty file" >config
	  dotest toplevel-cleanup-2 "${testcvs} -q ci -m toplevel-cleanup" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/top-dir ${CVSROOT_DIRNAME}/second-dir
	  ;;

	toplevel2)
	  # Similar to toplevel, but test the case where TopLevelAdmin=no.

	  # First set the TopLevelAdmin setting.
	  mkdir 1; cd 1
	  dotest toplevel2-1a "${testcvs} -q co CVSROOT/config" \
"U CVSROOT/config"
	  cd CVSROOT
	  echo "TopLevelAdmin=no" >config
	  dotest toplevel2-1b "${testcvs} -q ci -m no-top-level" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..
	  rm -r 1

	  # Now set up some directories and subdirectories
	  mkdir 1; cd 1
	  dotest toplevel2-1 "${testcvs} -q co -l ." ''
	  mkdir top-dir second-dir
	  dotest toplevel2-2 "${testcvs} add top-dir second-dir" \
"Directory ${CVSROOT_DIRNAME}/top-dir added to the repository
Directory ${CVSROOT_DIRNAME}/second-dir added to the repository"
	  cd top-dir

	  touch file1
	  dotest toplevel2-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest toplevel2-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/top-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/top-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..

	  cd second-dir
	  touch file2
	  dotest toplevel2-3s "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest toplevel2-4s "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/second-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/second-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  cd ../..
	  rm -r 1; mkdir 1; cd 1
	  dotest toplevel2-5 "${testcvs} co top-dir" \
"${PROG} checkout: Updating top-dir
U top-dir/file1"

	  dotest toplevel2-6 "${testcvs} update top-dir" \
"${PROG} update: Updating top-dir"
	  dotest toplevel2-7 "${testcvs} update"  \
"${PROG} update: Updating top-dir"

	  dotest toplevel2-8 "${testcvs} update -d top-dir" \
"${PROG} update: Updating top-dir"
	  # Contrast this with toplevel-9, which has TopLevelAdmin=yes.
	  dotest toplevel2-9 "${testcvs} update -d" \
"${PROG} update: Updating top-dir"

	  cd ..
	  rm -r 1; mkdir 1; cd 1
	  dotest toplevel2-10 "${testcvs} co top-dir" \
"${PROG} checkout: Updating top-dir
U top-dir/file1"
	  # This tests more or less the same thing, in a particularly
	  # "real life" example.  With TopLevelAdmin=yes, this command
	  # would give us second-dir and CVSROOT directories too.
	  dotest toplevel2-11 "${testcvs} -q update -d" ""

	  dotest toplevel2-cleanup-1 "${testcvs} -q co CVSROOT/config" \
"U CVSROOT/config"
	  cd CVSROOT
	  echo "# empty file" >config
	  dotest toplevel2-cleanup-2 "${testcvs} -q ci -m toplevel2-cleanup" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/top-dir ${CVSROOT_DIRNAME}/second-dir
	  ;;



	rstar-toplevel)
	  # This test used to confirm a bug that existed in the r* commands
	  # run against the top-level project prior to CVS 1.11.18 & 1.12.10.
	  #
	  # The assertion failure was something like:
	  # do_recursion: Assertion \`strstr (repository, \"/\./\") == ((void \*)0)' failed\..*"
	  dotest rstar-toplevel-1 "$testcvs -q rlog ." \
"
RCS file: $CVSROOT_DIRNAME/CVSROOT$DOTSTAR"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi
	;;



	trailingslashes)
	  # Some tests of CVS's reactions to path specifications containing
	  # trailing slashes.
	  mkdir trailingslashes; cd trailingslashes
	  dotest trailingslashes-init-1 "$testcvs -Q co -ldt ."
	  dotest trailingslashes-init-2 "$testcvs -Q co -dt2 ."
	  cd t
	  echo "Ahh'll be baaack." >topfile
	  dotest trailingslashes-init-3 "$testcvs -Q add topfile"
	  dotest trailingslashes-init-4 "$testcvs -Q ci -mto-top" \
"RCS file: $CVSROOT_DIRNAME/topfile,v
done
Checking in topfile;
$CVSROOT_DIRNAME/topfile,v  <--  topfile
initial revision: 1\.1
done"

	  # First, demonstrate the usual case.
	  cd ../t2
	  dotest trailingslashes-1 "$testcvs -q up CVSROOT"
	  dotest_fail trailingslashes-1a "test -f topfile"

	  # Now the one that used to fail in remote mode prior to 1.11.24
	  # & 1.12.14.  Formerly TODO item #205.
	  dotest trailingslashes-2 "$testcvs -q up CVSROOT/"
	  dotest_fail trailingslashes-2a "test -f topfile"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
	  rm -rf trailingslashes $CVSROOT_DIRNAME/topfile,v
	  ;;



        checkout_repository)
          dotest_fail checkout_repository-1 \
"${testcvs} co -d ${CVSROOT_DIRNAME} CVSROOT" \
"${PROG} \[checkout aborted\]: Cannot check out files into the repository itself" \
"${PROG} \[checkout aborted\]: absolute pathname \`${CVSROOT_DIRNAME}' illegal for server"

	  # The behavior of the client/server test below should be correct.
	  # The CVS client currently has no way of knowing that the client and
	  # server are the same machine and thus skips the $CVSROOT checks.
	  # I think checking for this case in CVS would be bloat since this
	  # should be a fairly rare occurance.
	  cd ${CVSROOT_DIRNAME}
          dotest_fail checkout_repository-2 "${testcvs} co CVSROOT" \
"${PROG} \[checkout aborted\]: Cannot check out files into the repository itself" \
"${PROG} checkout: Updating CVSROOT
${PROG} checkout: move away CVSROOT/checkoutlist; it is in the way
C CVSROOT/checkoutlist
${PROG} checkout: move away CVSROOT/commitinfo; it is in the way
C CVSROOT/commitinfo
${PROG} checkout: move away CVSROOT/config; it is in the way
C CVSROOT/config
${PROG} checkout: move away CVSROOT/cvswrappers; it is in the way
C CVSROOT/cvswrappers
${PROG} checkout: move away CVSROOT/editinfo; it is in the way
C CVSROOT/editinfo
${PROG} checkout: move away CVSROOT/loginfo; it is in the way
C CVSROOT/loginfo
${PROG} checkout: move away CVSROOT/modules; it is in the way
C CVSROOT/modules
${PROG} checkout: move away CVSROOT/notify; it is in the way
C CVSROOT/notify
${PROG} checkout: move away CVSROOT/rcsinfo; it is in the way
C CVSROOT/rcsinfo
${PROG} checkout: move away CVSROOT/taginfo; it is in the way
C CVSROOT/taginfo
${PROG} checkout: move away CVSROOT/verifymsg; it is in the way
C CVSROOT/verifymsg"

          dotest checkout_repository-3 \
"${testcvs} co -p CVSROOT/modules >/dev/null" \
"===================================================================
Checking out CVSROOT/modules
RCS:  ${CVSROOT_DIRNAME}/CVSROOT/modules,v
VERS: 1\.[0-9]*
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*"
	  cd ${TESTDIR}
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
sed <\$1 -e 's/^/x/' >${TESTDIR}/edit.new
mv ${TESTDIR}/edit.new \$1
exit 0
EOF
	  chmod +x ${TESTDIR}/editme

	  mkdir 1; cd 1
	  dotest editor-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest editor-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch file1 file2
	  dotest editor-3 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest editor-4 "${testcvs} -e ${TESTDIR}/editme -q ci" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  dotest editor-5 "${testcvs} -q tag -b br" "T file1
T file2"
	  dotest editor-6 "$testcvs -q update -r br" \
'U file1
U file2'
	  echo modify >>file1
	  dotest editor-7 "${testcvs} -e ${TESTDIR}/editme -q ci" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  # OK, now we want to make sure "ci -r" puts in the branch
	  # where appropriate.  Note that we can check in on the branch
	  # without being on the branch, because there is not a revision
	  # already on the branch.  If there were a revision on the branch,
	  # CVS would correctly give an up-to-date check failed.
	  dotest editor-8 "$testcvs -q update -A" \
'U file1
U file2'
	  echo add a line >>file2
	  dotest editor-9 "${testcvs} -q -e ${TESTDIR}/editme ci -rbr file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  dotest editor-log-file1 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
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
xCVS: ----------------------------------------------------------------------
xCVS: Enter Log.  Lines beginning with .CVS:. are removed automatically
xCVS:
xCVS: Modified Files:
xCVS:  Tag: br
xCVS: 	file2
xCVS: ----------------------------------------------------------------------
=============================================================================" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
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

	  # Test CVS's response to an unchanged log message
	  cat >${TESTDIR}/editme <<EOF
#!${TESTSHELL}
sleep 1
exit 0
EOF
	  chmod +x ${TESTDIR}/editme
	  dotest_fail editor-emptylog-1 "echo a |${testcvs} -e ${TESTDIR}/editme ci -f file1" \
"
Log message unchanged or not specified
a)bort, c)ontinue, e)dit, !)reuse this message unchanged for remaining dirs
Action: (continue) ${PROG} \[[a-z]* aborted\]: aborted by user"

	  # Test CVS's response to an empty log message
	  cat >${TESTDIR}/editme <<EOF
#!${TESTSHELL}
sleep 1
cat /dev/null >\$1
exit 0
EOF
	  chmod +x ${TESTDIR}/editme
	  dotest_fail editor-emptylog-1 "echo a |${testcvs} -e ${TESTDIR}/editme ci -f file1" \
"
Log message unchanged or not specified
a)bort, c)ontinue, e)dit, !)reuse this message unchanged for remaining dirs
Action: (continue) ${PROG} \[[a-z]* aborted\]: aborted by user"

	  # Test CVS's response to a log message with one blank line
	  cat >${TESTDIR}/editme <<EOF
#!${TESTSHELL}
sleep 1
echo >\$1
exit 0
EOF
	  chmod +x ${TESTDIR}/editme
	  dotest_fail editor-emptylog-1 "echo a |${testcvs} -e ${TESTDIR}/editme ci -f file1" \
"
Log message unchanged or not specified
a)bort, c)ontinue, e)dit, !)reuse this message unchanged for remaining dirs
Action: (continue) ${PROG} \[[a-z]* aborted\]: aborted by user"

	  # Test CVS's response to a log message with only comments
	  cat >${TESTDIR}/editme <<EOF
#!${TESTSHELL}
sleep 1
cat \$1 >${TESTDIR}/edit.new
mv ${TESTDIR}/edit.new \$1
exit 0
EOF
	  chmod +x ${TESTDIR}/editme
	  dotest_fail editor-emptylog-1 "echo a |${testcvs} -e ${TESTDIR}/editme ci -f file1" \
"
Log message unchanged or not specified
a)bort, c)ontinue, e)dit, !)reuse this message unchanged for remaining dirs
Action: (continue) ${PROG} \[[a-z]* aborted\]: aborted by user"

	  # Test CVS's response to a log message that is zero bytes
	  # in length. This caused core dumps in cvs 1.11.5 on Solaris
	  # hosts.
	  cd ..
	  dotest editor-emptylog-continue-1 "${testcvs} -q co CVSROOT/loginfo" \
"U CVSROOT/loginfo"

          cd CVSROOT
	  echo 'DEFAULT (echo Start-Log;cat;echo End-Log) >> \$CVSROOT/CVSROOT/commitlog' > loginfo
	  dotest editor-emptylog-continue-2 "${testcvs} commit -m add loginfo" \
"Checking in loginfo;
${CVSROOT_DIRNAME}/CVSROOT/loginfo,v  <--  loginfo
new revision: 1\.2; previous revision: 1\.1
done
${PROG} commit: Rebuilding administrative file database"

	  cd ../first-dir
	  cat >${TESTDIR}/editme <<EOF
#!${TESTSHELL}
sleep 1
cp /dev/null \$1
exit 1
EOF
	  chmod +x ${TESTDIR}/editme
	  dotest editor-emptylog-continue-3 "echo c |${testcvs} -e ${TESTDIR}/editme ci -f file1" \
"${PROG} [a-z]*: warning: editor session failed

Log message unchanged or not specified
a)bort, c)ontinue, e)dit, !)reuse this message unchanged for remaining dirs
Action: (continue) Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  # The loginfo Log message should be an empty line and not "(null)"
	  # which is what some fprintf() implementations do with "%s"
	  # format and a NULL pointer...
	  if $remote; then
	    dotest editor-emptylog-continue-4r \
"cat ${CVSROOT_DIRNAME}/CVSROOT/commitlog" \
"Start-Log
Update of ${CVSROOT_DIRNAME}/first-dir
In directory ${hostname}:${TMPDIR}/cvs-serv[0-9a-z]*

Modified Files:
	file1 
Log Message:

End-Log"
          else
	    dotest editor-emptylog-continue-4 \
"cat ${CVSROOT_DIRNAME}/CVSROOT/commitlog" \
"Start-Log
Update of ${CVSROOT_DIRNAME}/first-dir
In directory ${hostname}:${TESTDIR}/1/first-dir

Modified Files:
	file1 
Log Message:

End-Log"
         fi
	  # There should have an empty log message at this point
	  dotest editor-emptylog-continue-5 "${testcvs} log -N -r1.2 file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.2
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 3;	selected revisions: 1
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: +0 -0
\*\*\* empty log message \*\*\*
============================================================================="

	  # clean up
	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  restore_adm
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
	  # The second case in the local and remote versions of errmsg1-168
	  # below happens on Cygwin under Windows, where write privileges
	  # aren't enforced properly.
	  if $remote; then
	    dotest errmsg1-168r "${testcvs} -q update" \
"${PROG} update: foo is no longer in the repository
${PROG} update: unable to remove \./foo: Permission denied" \
"${PROG} update: foo is no longer in the repository"
	  else
	    dotest errmsg1-168 "${testcvs} -q update" \
"${PROG} update: foo is no longer in the repository
${PROG} update: unable to remove foo: Permission denied" \
"${PROG} update: foo is no longer in the repository"
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
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
"${PROG} add: cannot add special file .CVS.; skipping
${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  mkdir sdir
	  cd ..
	  dotest errmsg2-8 "${testcvs} add first-dir/sdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir added to the repository"
	  # while we're here... check commit with no CVS directory
	  dotest_fail errmsg2-8a "${testcvs} -q ci first-dir nonexistant" \
"${PROG} [a-z]*: nothing known about .nonexistant'
${PROG} \[[a-z]* aborted\]: correct above errors first!"
	  dotest_fail errmsg2-8b "${testcvs} -q ci nonexistant first-dir" \
"${PROG} [a-z]*: nothing known about .nonexistant'
${PROG} \[[a-z]* aborted\]: correct above errors first!"
	  dotest errmsg2-8c "${testcvs} -q ci first-dir" ""

	  cd first-dir

	  touch file10
	  mkdir sdir10
	  dotest errmsg2-10 "${testcvs} add file10 sdir10" \
"${PROG} add: scheduling file .file10. for addition
Directory ${CVSROOT_DIRNAME}/first-dir/sdir10 added to the repository
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest errmsg2-11 "${testcvs} -q ci -m add-file10" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file10,v
done
Checking in file10;
${CVSROOT_DIRNAME}/first-dir/file10,v  <--  file10
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
"Directory ${CVSROOT_DIRNAME}/first-dir/sdir10/ssdir added to the repository"

	  touch first-dir/sdir10/ssdir/ssfile
	  dotest errmsg2-14 \
	    "${testcvs} add first-dir/sdir10/ssdir/ssfile" \
"${PROG} add: scheduling file .first-dir/sdir10/ssdir/ssfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  touch first-dir/file15
	  dotest errmsg2-15 "${testcvs} add first-dir/file15" \
"${PROG} add: scheduling file .first-dir/file15. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"

	  # Now the case where we try to give it a directory which is not
	  # under CVS control.
	  mkdir bogus-dir
	  touch bogus-dir/file16
	  # The first message, from local CVS, is nice.  The second one
	  # is not nice; would be good to fix remote CVS to give a clearer
	  # message (e.g. the one from local CVS).  But at least it is an
	  # error message.
	  dotest_fail errmsg2-16 "${testcvs} add bogus-dir/file16" \
"${PROG} add: in directory bogus-dir:
${PROG} \[add aborted\]: there is no version here; do .${PROG} checkout. first" \
"${PROG} add: cannot open CVS/Entries for reading: No such file or directory
${PROG} \[add aborted\]: no repository"
	  rm -r bogus-dir

	  # One error condition we don't test for is trying to add a file
	  # or directory which already is there.

	  dotest errmsg2-17 "${testcvs} -q ci -m checkin" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file15,v
done
Checking in first-dir/file15;
${CVSROOT_DIRNAME}/first-dir/file15,v  <--  file15
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/sdir10/ssdir/ssfile,v
done
Checking in first-dir/sdir10/ssdir/ssfile;
${CVSROOT_DIRNAME}/first-dir/sdir10/ssdir/ssfile,v  <--  ssfile
initial revision: 1\.1
done"
	  dotest errmsg2-18 "${testcvs} -Q tag test" ''

	  # trying to import the repository

	  if $remote; then :; else
	    cd ${CVSROOT_DIRNAME}
	    dotest_fail errmsg2-20 "${testcvs} import -mtest . A B" \
"${PROG} \[import aborted\]: attempt to import the repository"
	    dotest_fail errmsg2-21 "${testcvs} import -mtest first-dir A B" \
"${PROG} \[import aborted\]: attempt to import the repository"
	  fi

	  cd ..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	adderrmsg)
	  # Test some of the error messages the 'add' command can return and
	  # their reactions to '-q'.

	  # First the usual setup; create a directory first-dir.
	  mkdir 1; cd 1
	  dotest adderrmsg-init1 "${testcvs} -q co -l ." ''
	  mkdir adderrmsg-dir
	  dotest adderrmsg-init2 "${testcvs} add adderrmsg-dir" \
"Directory ${CVSROOT_DIRNAME}/adderrmsg-dir added to the repository"
          cd adderrmsg-dir

	  # try to add the admin dir
	  dotest_fail adderrmsg-1 "${testcvs} add CVS" \
"${PROG} [a-z]*: cannot add special file .CVS.; skipping"
	  # might not want to see this message when you 'cvs add *'
	  dotest_fail adderrmsg-2 "${testcvs} -q add CVS" ""

	  # to test some other messages
	  touch file1
	  dotest adderrmsg-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"

	  # add it twice
	  dotest_fail adderrmsg-4 "${testcvs} add file1" \
"${PROG} add: file1 has already been entered"
	  dotest_fail adderrmsg-5 "${testcvs} -q add file1" ""

	  dotest adderrmsg-6 "${testcvs} -q ci -madd" \
"RCS file: ${CVSROOT_DIRNAME}/adderrmsg-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/adderrmsg-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  # file in Entries & repository
	  dotest_fail adderrmsg-7 "${testcvs} add file1" \
"${PROG} add: file1 already exists, with version number 1\.1"
	  dotest_fail adderrmsg-8 "${testcvs} -q add file1" ""

	  # clean up
	  cd ../..
	  if $keep; then :; else
	      rm -r 1
	      rm -rf ${CVSROOT_DIRNAME}/adderrmsg-dir
	  fi
	  ;;

	opterrmsg)
	  # Test some option parsing error messages

	  # No init is necessary since these error messages are printed b4
	  # CVS looks for a sandbox or repository

	  # -z used to accept non-numeric arguments.  This bit someone who
	  # attempted `cvs -z -n up' when the -n was read as the argument to
	  # -z.
	  dotest_fail opterrmsg-1 "${testcvs} -z -n up" \
"${PROG}: gzip compression level must be between 0 and 9"

	  # Some general -z checks
	  dotest_fail opterrmsg-2 "${testcvs} -z -1 up" \
"${PROG}: gzip compression level must be between 0 and 9"
	  dotest_fail opterrmsg-3 "${testcvs} -z10 up" \
"${PROG}: gzip compression level must be between 0 and 9"
	  ;;

	devcom)
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1
	  cd 1
	  dotest devcom-1 "${testcvs} -q co first-dir"

	  cd first-dir
	  echo abb >abb
	  dotest devcom-2 "${testcvs} add abb" \
"$PROG add: scheduling file \`abb' for addition
$PROG add: use '$PROG commit' to add this file permanently"

	  dotest devcom-3 "${testcvs} -q ci -m added" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/abb,v
done
Checking in abb;
${CVSROOT_DIRNAME}/first-dir/abb,v  <--  abb
initial revision: 1\.1
done"

	  dotest_fail devcom-4 "${testcvs} watch" "Usage${DOTSTAR}"

	  dotest devcom-5 "${testcvs} watch on"

	  echo abc >abc
	  dotest devcom-6 "${testcvs} add abc" \
"$PROG add: scheduling file \`abc' for addition
$PROG add: use '$PROG commit' to add this file permanently"

	  dotest devcom-7 "${testcvs} -q ci -m added" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/abc,v
done
Checking in abc;
${CVSROOT_DIRNAME}/first-dir/abc,v  <--  abc
initial revision: 1\.1
done"

	  cd ../..
	  mkdir 2
	  cd 2

	  dotest devcom-8 "${testcvs} -q co first-dir" \
"U first-dir/abb
U first-dir/abc"

	  cd first-dir
	  dotest_fail devcom-9 "test -w abb"
	  dotest_fail devcom-9 "test -w abc"

	  dotest devcom-10 "${testcvs} editors" ""

	  dotest devcom-11 "${testcvs} edit abb"

	  # Here we test for the traditional ISO C ctime() date format.
	  # We assume the C locale; I guess that works provided we set
	  # LC_ALL at the start of this script but whether these
	  # strings should vary based on locale does not strike me as
	  # self-evident.
	  dotest devcom-12 "${testcvs} editors" \
"abb	${username}	[SMTWF][uoehra][neduit] [JFAMSOND][aepuco][nbrylgptvc] [0-9 ][0-9] [0-9:]* [0-9][0-9][0-9][0-9] GMT	[-a-zA-Z_.0-9]*	${TESTDIR}/2/first-dir"

	  echo aaaa >>abb
	  dotest devcom-13 "${testcvs} ci -m modify abb" \
"Checking in abb;
${CVSROOT_DIRNAME}/first-dir/abb,v  <--  abb
new revision: 1\.2; previous revision: 1\.1
done"

	  # Unedit of a file not being edited should be a noop.
	  dotest devcom-14 "${testcvs} unedit abb" ''

	  dotest devcom-15 "${testcvs} editors" ""

	  dotest_fail devcom-16 "test -w abb"

	  dotest devcom-17 "${testcvs} edit abc"

	  # Unedit of an unmodified file.
	  dotest devcom-18 "${testcvs} unedit abc"
	  dotest devcom-19 "${testcvs} edit abc"

	  echo changedabc >abc
	  # Try to unedit a modified file; cvs should ask for confirmation
	  dotest devcom-20 "echo no | ${testcvs} unedit abc" \
"abc has been modified; revert changes? "

	  dotest devcom-21 "echo changedabc | cmp - abc"

	  # OK, now confirm the unedit
	  dotest devcom-22 "echo yes | ${testcvs} unedit abc" \
"abc has been modified; revert changes? "

	  dotest devcom-23 "echo abc | cmp - abc"

	  dotest devcom-24 "${testcvs} watchers" ''

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
	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/val-tags
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

	  if $keep; then
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

	  # Do a little error testing
	  dotest devcom2-18 "${testcvs} -q co -d first+dir first-dir" \
"U first${PLUS}dir/nw1
U first${PLUS}dir/w1
U first${PLUS}dir/w2
U first${PLUS}dir/w3"
	  cd first+dir
	  dotest_fail devcom2-19 "${testcvs} edit" \
"${PROG} \[[a-z]* aborted\]: current directory (${TESTDIR}/2/first${PLUS}dir) contains an invalid character (${PLUS},>;=\\\\t\\\\n)"

	  # Make sure there are no droppings lying around
	  dotest devcom2-20 "cat ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr" \
"Fw1	_watched=
Fw2	_watched=
Fw3	_watched=
Fnw1	_watched=
D	_watched="

	  cd ../..

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
	  cat <<'EOF' >>${CVSROOT_DIRNAME}/first-dir/CVS/fileattr
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

	  # Now test disconnected "cvs edit" and the format of the 
	  # CVS/Notify file.
	  if $remote; then
	    CVS_SERVER_save=${CVS_SERVER}
	    CVS_SERVER=${TESTDIR}/cvs-none; export CVS_SERVER

	    # The ${DOTSTAR} below matches the exact CVS server error message,
	    # which in :fork: mode is:
	    # "$PROG \[edit aborted\]: cannot exec $TESTDIR/cvs-none: ${DOTSTAR}",
	    # but which is:
	    # "bash2: line 1: $TESTDIR/cvs-none: No such file or directory"
	    # when testing across an :ext:/ssh link to my Linux 2.4 box.
	    #
	    # I can't even test for the second part of the error message,
	    # from the client, which varies more consistently, usually either
	    # "end of file from server" (if the process doing the exec exits
	    # before the parent gets around to sending data to it) or
	    # "received broken pipe signal" (if it is the other way around),
	    # since HP-UX fails to output it.
	    dotest_fail devcom3-9ar "${testcvs} edit w1 2>/dev/null"
	    dotest devcom3-9br "test -w w1" ""
	    dotest devcom3-9cr "cat CVS/Notify" \
"Ew1	[SMTWF][uoehra][neduit] [JFAMSOND][aepuco][nbrylgptvc] [0-9 ][0-9] [0-9:]* [0-9][0-9][0-9][0-9] GMT	[-a-zA-Z_.0-9]*	${TESTDIR}/1/first-dir	EUC"
	    CVS_SERVER=${CVS_SERVER_save}; export CVS_SERVER
	    dotest devcom3-9dr "${testcvs} -q update" ""
	    dotest_fail devcom3-9er "test -f CVS/Notify" ""
	    dotest devcom3-9fr "${testcvs} watchers w1" \
"w1	${username}	tedit	tunedit	tcommit"
	    dotest devcom3-9gr "${testcvs} unedit w1" ""
	    dotest devcom3-9hr "${testcvs} watchers w1" ""
	  fi

	  cd ../..
	  # OK, now change the tab to a space, and see that CVS gives
	  # a reasonable error (this is database corruption but CVS should
	  # not lose its mind).
	  sed -e 's/Fw2	/Fw2 /' <${CVSROOT_DIRNAME}/first-dir/CVS/fileattr \
	    >${CVSROOT_DIRNAME}/first-dir/CVS/fileattr.new
	  mv ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr.new \
	    ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr
	  mkdir 2; cd 2
	  dotest_fail devcom3-10 "${testcvs} -Q co ." \
"${PROG} \[checkout aborted\]: file attribute database corruption: tab missing in ${CVSROOT_DIRNAME}/first-dir/CVS/fileattr"
	  cd ..

	  # Use -f because of the readonly files.
	  rm -rf 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	watch4)
	  # More watch tests, including adding directories.
	  mkdir 1; cd 1
	  dotest watch4-0a "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest watch4-0b "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"

	  cd first-dir
	  dotest watch4-1 "${testcvs} watch on" ''
	  # This is just like the 173 test
	  touch file1
	  dotest watch4-2 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest watch4-3 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  # Now test the analogous behavior for directories.
	  mkdir subdir
	  dotest watch4-4 "${testcvs} add subdir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/subdir added to the repository"
	  cd subdir
	  touch sfile
	  dotest watch4-5 "${testcvs} add sfile" \
"${PROG} add: scheduling file .sfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest watch4-6 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/subdir/sfile,v
done
Checking in sfile;
${CVSROOT_DIRNAME}/first-dir/subdir/sfile,v  <--  sfile
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../..
	  cd 2/first-dir
	  dotest watch4-13 "${testcvs} -q update" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.2
Merging differences between 1\.1 and 1\.2 into file1
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in file1
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

	watch5)
	  # This test was designed to catch a problem in server
	  # mode where an 'cvs edit'd file disappeared from the
	  # CVS/Base directory when 'cvs status' or 'cvs update'
	  # was called on the file after the file was touched.
	  #
	  # This test is still here to prevent the bug from
	  # being reintroduced.
	  #
	  # The rationale for having CVS/Base stay around is that
	  # CVS/Base should be there if "cvs edit" has been run (this
	  # may be helpful as a "cvs editors" analogue, it is
	  # client-side and based on working directory not username;
	  # but more importantly, it isn't clear why a "cvs status"
	  # would act like an unedit, and even if it does, it would
	  # need to make the file read-only again).

	  mkdir watch5; cd watch5
	  dotest watch5-0a "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest watch5-0b "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"

	  cd first-dir
	  dotest watch5-1 "${testcvs} watch on" ''
	  # This is just like the 173 test
	  touch file1
	  dotest watch5-2 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest watch5-3 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest watch5-4 "${testcvs} edit file1" ''
	  dotest watch5-5 "test -f CVS/Base/file1" ''
	  if ${testcvs} status file1 >>${LOGFILE} 2>&1; then
		pass watch5-6
	  else
		fail watch5-6
	  fi
	  dotest watch5-7 "test -f CVS/Base/file1" ''

	  # Here's where the file used to dissappear
	  touch file1
	  if ${testcvs} status file1 >>${LOGFILE} 2>&1; then
		pass watch5-8
	  else
		fail watch5-8
	  fi
	  dotest watch5-10 "test -f CVS/Base/file1" ''

	  # Make sure update won't remove the file either
	  touch file1
	  dotest watch5-11 "${testcvs} -q up" ''
	  dotest watch5-12 "test -f CVS/Base/file1" ''

	  cd ../..
	  rm -r watch5
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;



	watch6)
	  # Check that `cvs watch on' does not reset the fileattr file.
	  mkdir watch6; cd watch6

	  dotest watch6-setup-1 "$testcvs -Q co -ldtop ."
	  cd top
	  mkdir watch6
	  dotest watch6-setup-2 "$testcvs -Q add watch6"

	  cd ..
	  dotest watch6-setup-3 "$testcvs -Q co watch6"
	  cd watch6

	  mkdir subdir
	  dotest watch6-setup-4 "$testcvs -Q add subdir"
	  cd subdir

	  # START watch add/remove sequence
	  dotest watch6-1 "$testcvs -Q watch add"
	  dotest watch6-2 \
"grep '_watchers' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"

	  dotest watch6-3 "$testcvs watch on"
	  dotest watch6-4 \
"grep '_watchers' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"
	  dotest watch6-5 \
"grep '_watched' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"

	  dotest watch6-6 "$testcvs watch off"
	  dotest watch6-7 \
"grep '_watchers' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"
	  dotest_fail watch6-8 \
"grep '_watched' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"

	  dotest watch6-9 "$testcvs watch remove"
	  dotest_fail watch6-10 \
"test -d $CVSROOT_DIRNAME/test-directory/subdir/CVS"
	  dotest_fail watch6-11 \
"test -f $CVSROOT_DIRNAME/test-directory/subdir/CVS/fileattr"
	  # END watch add/remove sequence

	  echo Hi there >afile
	  dotest watch6-12 "$testcvs -Q add afile"
	  dotest watch6-13 "$testcvs ci -m 'A file' afile" \
"RCS file: $CVSROOT_DIRNAME/watch6/subdir/afile,v
done
Checking in afile;
$CVSROOT_DIRNAME/watch6/subdir/afile,v  <--  afile
initial revision: 1\.1
done"

	  # START watch add/remove sequence
	  dotest watch6-14 "$testcvs -Q watch add"
	  dotest watch6-15 \
"grep '_watchers' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"

	  dotest watch6-16 "$testcvs watch on"
	  dotest watch6-17 \
"grep '_watchers' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"
	  dotest watch6-18 \
"grep '_watched' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"

	  dotest watch6-19 "$testcvs watch off"
	  dotest watch6-20 \
"grep '_watchers' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"
	  dotest_fail watch6-21 \
"grep '_watched' $CVSROOT_DIRNAME/watch6/subdir/CVS/fileattr >/dev/null"

	  dotest watch6-22 "$testcvs watch remove"
	  dotest_fail watch6-23 \
"test -d $CVSROOT_DIRNAME/test-directory/subdir/CVS"
	  dotest_fail watch6-24 \
"test -f $CVSROOT_DIRNAME/test-directory/subdir/CVS/fileattr"
	  # END watch add/remove sequence

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi
	  cd ../../..
	  rm -r watch6
	  rm -rf $CVSROOT_DIRNAME/watch6
	  ;;



	unedit-without-baserev)
	  mkdir 1; cd 1
	  module=x

	  file=m
	  echo foo > $file
	  dotest unedit-without-baserev-1 \
	    "$testcvs -Q import -m . $module X Y" ''
	  dotest unedit-without-baserev-2 "$testcvs -Q co $module" ''
	  cd $module

	  dotest unedit-without-baserev-3 "$testcvs -Q edit $file" ''

	  echo add a line >> $file
	  rm -f CVS/Baserev

	  # This will fail on most systems.
	  echo "yes" | dotest unedit-without-baserev-4 "${testcvs} -Q unedit $file" \
"m has been modified; revert changes${QUESTION} ${PROG} unedit: m not mentioned in CVS/Baserev
${PROG} unedit: run update to complete the unedit"

	  # SunOS4.1.4 systems make it this far, but with a corrupted
	  # CVS/Entries file.  Demonstrate the corruption!
	  dotest unedit-without-baserev-5 "cat CVS/Entries" \
	    "/$file/1\.1\.1\.1/${DOTSTAR}"

	  dotest unedit-without-baserev-6 "${testcvs} -q update" \
"${PROG} update: warning: m was lost
U m"

	  # OK, those were the easy cases.  Now tackle the hard one
	  # (the reason that CVS/Baserev was invented rather than just
	  # getting the revision from CVS/Entries).  This is very
	  # similar to watch4-10 through watch4-18 but with Baserev
	  # missing.
	  cd ../..
	  mkdir 2; cd 2
	  dotest unedit-without-baserev-7 "${testcvs} -Q co x" ''
	  cd x

	  dotest unedit-without-baserev-10 "${testcvs} edit m" ''
	  echo 'edited in 2' >m
	  cd ../..

	  cd 1/x
	  dotest unedit-without-baserev-11 "${testcvs} edit m" ''
	  echo 'edited in 1' >m
	  dotest unedit-without-baserev-12 "${testcvs} -q ci -m edit-in-1" \
"Checking in m;
${CVSROOT_DIRNAME}/x/m,v  <--  m
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../..
	  cd 2/x
	  dotest unedit-without-baserev-13 "${testcvs} -q update" \
"RCS file: ${CVSROOT_DIRNAME}/x/m,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.2
Merging differences between 1\.1\.1\.1 and 1\.2 into m
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in m
C m"
	  rm CVS/Baserev
	  dotest unedit-without-baserev-14 "echo yes | ${testcvs} unedit m" \
"m has been modified; revert changes${QUESTION} ${PROG} unedit: m not mentioned in CVS/Baserev
${PROG} unedit: run update to complete the unedit"
	  dotest unedit-without-baserev-15 "${testcvs} -q update" \
"${PROG} update: warning: m was lost
U m"
	  # The following tests are kind of degenerate compared with
	  # watch4-16 through watch4-18 but might as well make sure that
	  # nothing seriously wrong has happened to the working directory.
	  dotest unedit-without-baserev-16 "cat m" 'edited in 1'
	  # Make sure CVS really thinks we are at 1.2.
	  dotest unedit-without-baserev-17 "${testcvs} -q update" ""
	  dotest unedit-without-baserev-18 "cat m" "edited in 1"

	  cd ../..
	  rm -rf 1
	  rm -r 2
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	ignore)
	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir ignore
	  cd ignore

	  dotest ignore-1 "${testcvs} -q co CVSROOT" "U CVSROOT/${DOTSTAR}"
	  cd CVSROOT
	  echo rootig.c >cvsignore
	  dotest ignore-2 "${testcvs} add cvsignore" \
"${PROG}"' add: scheduling file `cvsignore'"'"' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  # As of Jan 96, local CVS prints "Examining ." and remote doesn't.
	  # Accept either.
	  dotest ignore-3 " ${testcvs} ci -m added" \
"${PROG} [a-z]*: Examining \.
RCS file: ${CVSROOT_DIRNAME}/CVSROOT/cvsignore,v
done
Checking in cvsignore;
${CVSROOT_DIRNAME}/CVSROOT/cvsignore,v  <--  cvsignore
initial revision: 1\.1
done
${PROG} commit: Rebuilding administrative file database"

	  cd ..
	  if echo "yes" | ${testcvs} release -d CVSROOT >>${LOGFILE} ; then
	      pass ignore-4
	  else
	      fail ignore-4
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
	  dotest_sort ignore-5 "${testcvs} import -m m -I optig.c ignore/first-dir tag1 tag2" \
'

I ignore/first-dir/defig.o
I ignore/first-dir/envig.c
I ignore/first-dir/optig.c
I ignore/first-dir/rootig.c
N ignore/first-dir/bar.c
N ignore/first-dir/foobar.c
No conflicts created by this import'
	  dotest_sort ignore-6 "${testcvs} import -m m -I ! ignore/second-dir tag3 tag4" \
'

N ignore/second-dir/bar.c
N ignore/second-dir/defig.o
N ignore/second-dir/envig.c
N ignore/second-dir/foobar.c
N ignore/second-dir/optig.c
N ignore/second-dir/rootig.c
No conflicts created by this import'
	  cd ..
	  rm -r dir-to-import

	  mkdir 1
	  cd 1
	  dotest ignore-7 "${testcvs} -q co -dsecond-dir ignore/second-dir" \
'U second-dir/bar.c
U second-dir/defig.o
U second-dir/envig.c
U second-dir/foobar.c
U second-dir/optig.c
U second-dir/rootig.c'
	  dotest ignore-8 "${testcvs} -q co -dfirst-dir ignore/first-dir" 'U first-dir/bar.c
U first-dir/foobar.c'
	  cd first-dir
	  touch rootig.c defig.o envig.c optig.c notig.c
	  dotest ignore-9 "${testcvs} -q update -I optig.c" "${QUESTION} notig.c"
	  # The fact that CVS requires us to specify -I CVS here strikes me
	  # as a bug.
	  dotest_sort ignore-10 "${testcvs} -q update -I ! -I CVS" \
"${QUESTION} defig.o
${QUESTION} envig.c
${QUESTION} notig.c
${QUESTION} optig.c
${QUESTION} rootig.c"

	  # Now test that commands other than update also print "? notig.c"
	  # where appropriate.  Only test this for remote, because local
	  # CVS only prints it on update.
	  rm optig.c
	  if $remote; then
	    dotest ignore-11r "${testcvs} -q diff" "${QUESTION} notig.c"

	    # Force the server to be contacted.  Ugh.  Having CVS
	    # contact the server for the sole purpose of checking
	    # the CVSROOT/cvsignore file does not seem like such a
	    # good idea, so I imagine this will continue to be
	    # necessary.  Oh well, at least we test CVS's ablity to
	    # handle a file with a modified timestamp but unmodified
	    # contents.
	    touch bar.c

	    dotest ignore-11r "${testcvs} -q ci -m commit-it" "${QUESTION} notig.c"
	  fi

	  # now test .cvsignore files
	  cd ..
	  echo notig.c >first-dir/.cvsignore
	  echo foobar.c >second-dir/.cvsignore
	  touch first-dir/notig.c second-dir/notig.c second-dir/foobar.c
	  dotest_sort ignore-12 "${testcvs} -qn update" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c"
	  dotest_sort ignore-13 "${testcvs} -qn update -I! -I CVS" \
"${QUESTION} first-dir/.cvsignore
${QUESTION} first-dir/defig.o
${QUESTION} first-dir/envig.c
${QUESTION} first-dir/rootig.c
${QUESTION} second-dir/.cvsignore
${QUESTION} second-dir/notig.c"

	  echo yes | dotest ignore-14 "${testcvs} release -d first-dir" \
"${QUESTION} \.cvsignore
You have \[0\] altered files in this repository.
Are you sure you want to release (and delete) directory .first-dir': "

	  echo add a line >>second-dir/foobar.c
	  rm second-dir/notig.c second-dir/.cvsignore
	  echo yes | dotest ignore-15 "${testcvs} release -d second-dir" \
"M foobar.c
You have \[1\] altered files in this repository.
Are you sure you want to release (and delete) directory .second-dir': "

	  cd ../..
	  if $keep; then :; else
	    rm -r ignore
	    rm -rf ${CVSROOT_DIRNAME}/ignore
	  fi
	  ;;

	ignore-on-branch)
	  # Test that CVS _doesn't_ ignore files on branches because they were
	  # added to the trunk.
	  mkdir ignore-on-branch; cd ignore-on-branch
	  mkdir $CVSROOT_DIRNAME/ignore-on-branch

	  # create file1 & file2 on trunk
	  dotest ignore-on-branch-setup-1 "$testcvs -q co -dsetup ignore-on-branch" ''
	  cd setup
	  echo file1 >file1 
	  dotest ignore-on-branch-setup-2 "$testcvs -q add file1" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest ignore-on-branch-setup-3 "$testcvs -q ci -mfile1 file1" \
"RCS file: $CVSROOT_DIRNAME/ignore-on-branch/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/ignore-on-branch/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest ignore-on-branch-setup-4 "$testcvs -q tag -b branch" 'T file1'
	  echo file2 >file2 
	  dotest ignore-on-branch-setup-5 "$testcvs -q add file2" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest ignore-on-branch-setup-6 "$testcvs -q ci -mtrunk file2" \
"RCS file: $CVSROOT_DIRNAME/ignore-on-branch/file2,v
done
Checking in file2;
$CVSROOT_DIRNAME/ignore-on-branch/file2,v  <--  file2
initial revision: 1\.1
done"

	  cd ..

	  # Check out branch.
	  #
	  # - This was the original failure case - file2 would not be flagged
	  #   with a '?'
	  dotest ignore-on-branch-1 "$testcvs -q co -rbranch ignore-on-branch" \
'U ignore-on-branch/file1'
	  cd ignore-on-branch
	  echo file2 on branch >file2 
	  dotest ignore-on-branch-2 "$testcvs -nq update" '? file2'

	  # Now set up for a join.  One of the original fixes for this would
	  # print out a 'U' and a '?' during a join which added a file.
	  if $remote; then
	    dotest ignore-on-branch-3 "$testcvs -q tag -b branch2" \
'? file2
T file1'
	  else
	    dotest ignore-on-branch-3 "$testcvs -q tag -b branch2" 'T file1'
	  fi
	  dotest ignore-on-branch-4 "$testcvs -q add file2" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest ignore-on-branch-5 "$testcvs -q ci -mbranch file2" \
"Checking in file2;
$CVSROOT_DIRNAME/ignore-on-branch/file2,v  <--  file2
new revision: 1\.1\.2\.2; previous revision: 1\.1\.2\.1
done"
	  dotest ignore-on-branch-6 "$testcvs -q up -rbranch2" \
"[UP] file1
$PROG update: file2 is no longer in the repository"
	  dotest ignore-on-branch-7 "$testcvs -q up -jbranch" 'U file2'

	  cd ../..
	  if $keep; then :; else
	    rm -r ignore-on-branch
	    rm -rf $CVSROOT_DIRNAME/ignore-on-branch
	  fi
	  ;;

	binfiles)
	  # Test cvs's ability to handle binary files.
	  # List of binary file tests:
	  #   * conflicts, "cvs admin": binfiles
	  #   * branching and joining: binfiles2
	  #   * adding and removing files: binfiles3
	  #   * -k wrappers: binwrap, binwrap2, binwrap3
	  #   * "cvs import" and wrappers: binwrap, binwrap2, binwrap3
	  #   * -k option to "cvs import": none yet, as far as I know.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest binfiles-1 "${testcvs} -q co first-dir" ''
	  ${AWK} 'BEGIN { printf "%c%c%c@%c%c", 2, 10, 137, 13, 10 }' \
	    </dev/null | ${TR} '@' '\000' >binfile.dat
	  cat binfile.dat binfile.dat >binfile2.dat
	  cd first-dir
	  cp ../binfile.dat binfile
	  dotest binfiles-2 "${testcvs} add -kb binfile" \
"${PROG}"' add: scheduling file `binfile'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest binfiles-3 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/binfile,v
done
Checking in binfile;
${CVSROOT_DIRNAME}/first-dir/binfile,v  <--  binfile
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
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"

	  # Test that "-kk" does not override "-kb"
	  cd ../..
	  mkdir 2a; cd 2a
	  dotest binfiles-5.5a0 "${testcvs} -q co -kk first-dir" 'U first-dir/binfile'
	  cd first-dir
	  # Testing that sticky options is -kb is the closest thing we have
	  # to testing that binary files work right on non-unix machines
	  # (until there is automated testing for such machines, of course).
	  dotest binfiles-5.5a1 "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/binfile,v
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
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  cd ../..
	  rm -r 3
	  # test that "-kk" does not override "-kb"
	  mkdir 3; cd 3
	  dotest binfiles-5.5c0 "${testcvs} -q co -kk first-dir/binfile" \
'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-5.5c1 "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  cd ../..
	  rm -r 3
	  cd 2/first-dir

	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-6 "${testcvs} -q ci -m modify-it" \
"Checking in binfile;
${CVSROOT_DIRNAME}/first-dir/binfile,v  <--  binfile
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../../1/first-dir
	  dotest binfiles-7 "${testcvs} -q update" '[UP] binfile'
	  dotest binfiles-8 "cmp ../binfile2.dat binfile" ''

	  # Now test handling of conflicts with binary files.
	  cp ../binfile.dat binfile
	  dotest binfiles-con0 "${testcvs} -q ci -m modify-it" \
"Checking in binfile;
${CVSROOT_DIRNAME}/first-dir/binfile,v  <--  binfile
new revision: 1\.3; previous revision: 1\.2
done"
	  cd ../../2/first-dir
	  echo 'edits in dir 2' >binfile
	  dotest binfiles-con1 "${testcvs} -q update" \
"$PROG update: nonmergeable file needs merge
${PROG} update: revision 1\.3 from repository is now in binfile
${PROG} update: file from working directory is now in \.#binfile\.1\.2
C binfile"

	  dotest_fail binfiles-con1b "$testcvs -q up" "C binfile"

	  dotest binfiles-con2 "cmp binfile ../../1/binfile.dat" ''
	  dotest binfiles-con3 "cat .#binfile.1.2" 'edits in dir 2'

	  cp ../../1/binfile2.dat binfile
	  dotest binfiles-con4 "${testcvs} -q ci -m resolve-it" \
"Checking in binfile;
${CVSROOT_DIRNAME}/first-dir/binfile,v  <--  binfile
new revision: 1\.4; previous revision: 1\.3
done"
	  cd ../../1/first-dir
	  dotest binfiles-con5 "${testcvs} -q update" '[UP] binfile'

	  dotest binfiles-9 "${testcvs} -q update -A" ''
	  # "-kk" no longer does anything with "-kb"
	  dotest binfiles-10 "${testcvs} -q update -kk" ''
	  dotest binfiles-11 "${testcvs} -q update" ''
	  # "-kk" no longer does anything with "-kb"
	  dotest binfiles-12 "${testcvs} -q update -A" ''
	  dotest binfiles-13 "${testcvs} -q update -A" ''

	  cd ../..

	  mkdir 3
	  cd 3
	  dotest binfiles-13a0 "${testcvs} -q co -r HEAD first-dir" \
'U first-dir/binfile'
	  cd first-dir
	  dotest binfiles-13a1 "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.4.*
   Repository revision:	1\.4	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		HEAD (revision: 1\.4)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  cd ../..
	  rm -r 3

	  cd 2/first-dir
	  echo 'this file is $''RCSfile$' >binfile
	  dotest binfiles-14a "${testcvs} -q ci -m modify-it" \
"Checking in binfile;
${CVSROOT_DIRNAME}/first-dir/binfile,v  <--  binfile
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest binfiles-14b "cat binfile" 'this file is $''RCSfile$'
	  # See binfiles-5.5 for discussion of -kb.
	  dotest binfiles-14c "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  dotest binfiles-14d "${testcvs} admin -kv binfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/binfile,v
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
   Repository revision:	1\.5	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"
	  dotest binfiles-14g "${testcvs} -q update -A" '[UP] binfile'
	  dotest binfiles-14h "cat binfile" 'this file is binfile,v'
	  dotest binfiles-14i "${testcvs} status binfile" \
"===================================================================
File: binfile          	Status: Up-to-date

   Working revision:	1\.5.*
   Repository revision:	1\.5	${CVSROOT_DIRNAME}/first-dir/binfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kv"

	  # Do sticky options work when used with 'cvs update'?
	  echo "Not a binary file." > nibfile
	  dotest binfiles-sticky1 "${testcvs} -q add nibfile" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest binfiles-sticky2 "${testcvs} -q ci -m add-it nibfile" \
	    "RCS file: ${CVSROOT_DIRNAME}/first-dir/nibfile,v
done
Checking in nibfile;
${CVSROOT_DIRNAME}/first-dir/nibfile,v  <--  nibfile
initial revision: 1\.1
done"
	  dotest binfiles-sticky3 "${testcvs} -q update -kb nibfile" \
	    '[UP] nibfile'
	  dotest binfiles-sticky4 "${testcvs} -q status nibfile" \
"===================================================================
File: nibfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/nibfile,v
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
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/nibfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest binfiles-15 "${testcvs} -q admin -kb nibfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/nibfile,v
done"
	  dotest binfiles-16 "${testcvs} -q update nibfile" "[UP] nibfile"
	  dotest binfiles-17 "${testcvs} -q status nibfile" \
"===================================================================
File: nibfile          	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/nibfile,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-kb"

	  dotest binfiles-o1 "${testcvs} admin -o1.3:: binfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/binfile,v
deleting revision 1\.5
deleting revision 1\.4
done"
	  dotest binfiles-o2 "${testcvs} admin -o::1.3 binfile" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/binfile,v
deleting revision 1\.2
deleting revision 1\.1
done"
	  dotest binfiles-o3 "${testcvs} -q log -h -N binfile" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/binfile,v
Working file: binfile
head: 1\.3
branch:
locks: strict
access list:
keyword substitution: v
total revisions: 1
============================================================================="

	  # Check that the contents were right.  This isn't the hard case
	  # (in which RCS_delete_revs does a diff), but might as well.
	  dotest binfiles-o4 "${testcvs} -q update binfile" "U binfile"
	  dotest binfiles-o5 "cmp binfile ../../1/binfile.dat" ""

	  cd ../..
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  rm -r 1
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
	  ${AWK} 'BEGIN { printf "%c%c%c@%c%c", 2, 10, 137, 13, 10 }' \
	    </dev/null | ${TR} '@' '\000' >../binfile
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
"${PROG} add: scheduling file .brmod. for addition
${PROG} add: scheduling file .brmod-trmod. for addition
${PROG} add: scheduling file .brmod-wdmod. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest binfiles2-1b "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod,v
done
Checking in brmod;
${CVSROOT_DIRNAME}/first-dir/brmod,v  <--  brmod
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v
done
Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v
done
Checking in brmod-wdmod;
${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v  <--  brmod-wdmod
initial revision: 1\.1
done"
	  dotest binfiles2-2 "${testcvs} -q tag -b br" 'T brmod
T brmod-trmod
T brmod-wdmod'
	  dotest binfiles2-3 "$testcvs -q update -r br" \
'U brmod
U brmod-trmod
U brmod-wdmod'
	  cp ../binfile binfile.dat
	  dotest binfiles2-4 "${testcvs} add -kb binfile.dat" \
"${PROG} add: scheduling file .binfile\.dat. for addition on branch .br.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  cp ../binfile2 brmod
	  cp ../binfile2 brmod-trmod
	  cp ../binfile2 brmod-wdmod
	  dotest binfiles2-5 "${testcvs} -q ci -m br-changes" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/binfile\.dat,v
done
Checking in binfile\.dat;
${CVSROOT_DIRNAME}/first-dir/Attic/binfile\.dat,v  <--  binfile\.dat
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod;
${CVSROOT_DIRNAME}/first-dir/brmod,v  <--  brmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-wdmod;
${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest binfiles2-6 "${testcvs} -q update -A" \
"${PROG} update: binfile\.dat is no longer in the repository
[UP] brmod
[UP] brmod-trmod
[UP] brmod-wdmod"
	  dotest_fail binfiles2-7 "test -f binfile.dat" ''
	  dotest binfiles2-7-brmod "cmp ../binfile brmod"
	  cp ../binfile3 brmod-trmod
	  dotest binfiles2-7a "${testcvs} -q ci -m tr-modify" \
"Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.2; previous revision: 1\.1
done"
	  cp ../binfile3 brmod-wdmod

	  dotest binfiles2-8 "${testcvs} -q update -j br" \
"U binfile\.dat
U brmod
${PROG} update: nonmergeable file needs merge
${PROG} update: revision 1.1.2.1 from repository is now in brmod-trmod
${PROG} update: file from working directory is now in .#brmod-trmod.1.2
C brmod-trmod
M brmod-wdmod
${PROG} update: nonmergeable file needs merge
${PROG} update: revision 1.1.2.1 from repository is now in brmod-wdmod
${PROG} update: file from working directory is now in .#brmod-wdmod.1.1
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
${CVSROOT_DIRNAME}/first-dir/binfile\.dat,v  <--  binfile\.dat
new revision: 1\.2; previous revision: 1\.1
done
Checking in brmod;
${CVSROOT_DIRNAME}/first-dir/brmod,v  <--  brmod
new revision: 1\.2; previous revision: 1\.1
done
Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.3; previous revision: 1\.2
done
Checking in brmod-wdmod;
${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.2; previous revision: 1\.1
done"

	  dotest_fail binfiles2-o1 "${testcvs} -q admin -o :1.2 brmod-trmod" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v
deleting revision 1\.2
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v: can't remove branch point 1\.1
${PROG} admin: RCS file for .brmod-trmod. not modified\."
	  dotest binfiles2-o2 "${testcvs} -q admin -o 1.1.2.1: brmod-trmod" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v
deleting revision 1\.1\.2\.1
done"
	  dotest binfiles2-o3 "${testcvs} -q admin -o :1.2 brmod-trmod" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v
deleting revision 1\.2
deleting revision 1\.1
done"
	  dotest binfiles2-o4 "${testcvs} -q log -N brmod-trmod" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v
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

	binfiles3)
	  # More binary file tests, especially removing, adding, &c.
	  # See "binfiles" for a list of binary file tests.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  mkdir 1; cd 1
	  dotest binfiles3-1 "${testcvs} -q co first-dir" ''
	  ${AWK} 'BEGIN { printf "%c%c%c@%c%c", 2, 10, 137, 13, 10 }' \
	    </dev/null | ${TR} '@' '\000' >binfile.dat
	  cd first-dir
	  echo hello >file1
	  dotest binfiles3-2 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest binfiles3-3 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  rm file1
	  dotest binfiles3-4 "${testcvs} rm file1" \
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest binfiles3-5 "${testcvs} -q ci -m remove-it" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.1
done"
	  cp ../binfile.dat file1
	  dotest binfiles3-6 "${testcvs} add -kb file1" \
"${PROG} add: Re-adding file .file1. (in place of dead revision 1\.2)\.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  # The idea behind this test is to make sure that the file
	  # gets opened in binary mode to send to "cvs ci".
	  dotest binfiles3-6a "cat CVS/Entries" \
"/file1/0/[A-Za-z0-9 :]*/-kb/
D"
	  # TODO: This just tests the case where the old keyword
	  # expansion mode is the default (RCS_getexpand == NULL
	  # in checkaddfile()); should also test the case in which
	  # we are changing it from one non-default value to another.
	  dotest binfiles3-7 "${testcvs} -q ci -m readd-it" \
"${PROG} commit: changing keyword expansion mode to -kb
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest binfiles3-8 "${testcvs} -q log -h -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:
keyword substitution: b
total revisions: 3
============================================================================="

	  # OK, now test admin -o on a binary file.  See "admin"
	  # test for a more complete list of admin -o tests.
	  cp ${TESTDIR}/1/binfile.dat ${TESTDIR}/1/binfile4.dat
	  echo '%%$$##@@!!jjiiuull' | ${TR} j '\000' >>${TESTDIR}/1/binfile4.dat
	  cp ${TESTDIR}/1/binfile4.dat ${TESTDIR}/1/binfile5.dat
	  echo 'aawwee%$$##@@!!jjil' | ${TR} w '\000' >>${TESTDIR}/1/binfile5.dat

	  cp ../binfile4.dat file1
	  dotest binfiles3-9 "${testcvs} -q ci -m change" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"
	  cp ../binfile5.dat file1
	  dotest binfiles3-10 "${testcvs} -q ci -m change" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest binfiles3-11 "${testcvs} admin -o 1.3::1.5 file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
deleting revision 1\.4
done"
	  dotest binfiles3-12 "${testcvs} -q update -r 1.3 file1" "U file1"
	  dotest binfiles3-13 "cmp file1 ${TESTDIR}/1/binfile.dat" ""

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
	  if $remote; then :; else

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
"${PROG} add: scheduling file .\.cvswrappers. for addition
${PROG} add: scheduling file .brmod. for addition
${PROG} add: scheduling file .brmod-trmod. for addition
${PROG} add: scheduling file .brmod-wdmod. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	    dotest mcopy-1b "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/\.cvswrappers,v
done
Checking in \.cvswrappers;
${CVSROOT_DIRNAME}/first-dir/\.cvswrappers,v  <--  \.cvswrappers
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod,v
done
Checking in brmod;
${CVSROOT_DIRNAME}/first-dir/brmod,v  <--  brmod
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v
done
Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v
done
Checking in brmod-wdmod;
${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v  <--  brmod-wdmod
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
	    dotest mcopy-3 "$testcvs -q update -r br" \
'U .cvswrappers
U brmod
U brmod-trmod
U brmod-wdmod'
	    echo 'modify brmod on br' >brmod
	    echo 'modify brmod-trmod on br' >brmod-trmod
	    echo 'modify brmod-wdmod on br' >brmod-wdmod
	    dotest mcopy-5 "${testcvs} -q ci -m br-changes" \
"Checking in brmod;
${CVSROOT_DIRNAME}/first-dir/brmod,v  <--  brmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
Checking in brmod-wdmod;
${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v  <--  brmod-wdmod
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	    dotest mcopy-6 "$testcvs -q update -A" \
'U .cvswrappers
U brmod
U brmod-trmod
U brmod-wdmod'
	    dotest mcopy-7 "cat brmod brmod-trmod brmod-wdmod" \
"brmod initial contents
brmod-trmod initial contents
brmod-wdmod initial contents"

	    echo 'modify brmod-trmod again on trunk' >brmod-trmod
	    dotest mcopy-7a "${testcvs} -q ci -m tr-modify" \
"Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.2; previous revision: 1\.1
done"
	    echo 'modify brmod-wdmod in working dir' >brmod-wdmod

	    dotest mcopy-8 "${testcvs} -q update -j br" \
"U brmod
${PROG} update: nonmergeable file needs merge
${PROG} update: revision 1.1.2.1 from repository is now in brmod-trmod
${PROG} update: file from working directory is now in .#brmod-trmod.1.2
C brmod-trmod
M brmod-wdmod
${PROG} update: nonmergeable file needs merge
${PROG} update: revision 1.1.2.1 from repository is now in brmod-wdmod
${PROG} update: file from working directory is now in .#brmod-wdmod.1.1
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
${CVSROOT_DIRNAME}/first-dir/brmod,v  <--  brmod
new revision: 1\.2; previous revision: 1\.1
done
Checking in brmod-trmod;
${CVSROOT_DIRNAME}/first-dir/brmod-trmod,v  <--  brmod-trmod
new revision: 1\.3; previous revision: 1\.2
done
Checking in brmod-wdmod;
${CVSROOT_DIRNAME}/first-dir/brmod-wdmod,v  <--  brmod-wdmod
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
   Repository revision:	1\.1\.1\.1	${CVSROOT_DIRNAME}/first-dir/foo\.c,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: foo\.exe          	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	${CVSROOT_DIRNAME}/first-dir/foo\.exe,v
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
   Repository revision:	1\.1\.1\.1	${CVSROOT_DIRNAME}/first-dir/foo\.c,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	-ko

===================================================================
File: foo\.exe          	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	${CVSROOT_DIRNAME}/first-dir/foo\.exe,v
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
          
          echo "bar*" > binwrap3/.cvswrappers
          echo "*.c0 -k 'b'" >> binwrap3/.cvswrappers
          echo "whatever -k 'b'" >> binwrap3/.cvswrappers
          echo ${binwrap3_text} > binwrap3/foo-b.c0
          echo ${binwrap3_text} > binwrap3/bar-t.c0
          echo ${binwrap3_text} > binwrap3/foo-b.sb
          echo ${binwrap3_text} > binwrap3/foo-t.sb
          echo ${binwrap3_text} > binwrap3/foo-t.c1
          echo ${binwrap3_text} > binwrap3/foo-t.st

          echo "bar* -k 'kv'" > binwrap3/sub1/.cvswrappers
          echo "*.c1 -k 'b'" >> binwrap3/sub1/.cvswrappers
          echo "whatever -k 'b'" >> binwrap3/sub1/.cvswrappers
          echo ${binwrap3_text} > binwrap3/sub1/foo-b.c1
          echo ${binwrap3_text} > binwrap3/sub1/bar-t.c1
          echo ${binwrap3_text} > binwrap3/sub1/foo-b.sb
          echo ${binwrap3_text} > binwrap3/sub1/foo-t.sb
          echo ${binwrap3_text} > binwrap3/sub1/foo-t.c0
          echo ${binwrap3_text} > binwrap3/sub1/foo-t.st

          echo "bar*" > binwrap3/sub2/.cvswrappers
          echo "*.st -k 'b'" >> binwrap3/sub2/.cvswrappers
          echo ${binwrap3_text} > binwrap3/sub2/foo-b.sb
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.sb
          echo ${binwrap3_text} > binwrap3/sub2/foo-b.st
          echo ${binwrap3_text} > binwrap3/sub2/bar-t.st
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c0
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c1
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c2
          echo ${binwrap3_text} > binwrap3/sub2/foo-t.c3

          echo "bar* -k 'kv'" > binwrap3/sub2/subsub/.cvswrappers
          echo "*.c3 -k 'b'" >> binwrap3/sub2/subsub/.cvswrappers
          echo "foo -k 'b'" >> binwrap3/sub2/subsub/.cvswrappers
          echo "c0* -k 'b'" >> binwrap3/sub2/subsub/.cvswrappers
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-b.c3
          echo ${binwrap3_text} > binwrap3/sub2/subsub/bar-t.c3
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-b.sb
          echo ${binwrap3_text} > binwrap3/sub2/subsub/foo-t.sb
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
	  echo "foo-t.sb" > cvswrappers
	  echo "foo*.sb  -k 'b'" >> cvswrappers
	  dotest binwrap3-2 "${testcvs} -q ci -m cvswrappers-mod" \
"Checking in cvswrappers;
${CVSROOT_DIRNAME}/CVSROOT/cvswrappers,v  <--  cvswrappers
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
          cd ..

          # Avoid environmental interference
          CVSWRAPPERS_save=${CVSWRAPPERS}
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
"${PROG} add: scheduling file .file1\.newbin. for addition
${PROG} add: scheduling file .file1\.txt. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest binwrap3-2d "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/binwrap3/sub2/file1\.newbin,v
done
Checking in file1\.newbin;
${CVSROOT_DIRNAME}/binwrap3/sub2/file1\.newbin,v  <--  file1\.newbin
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/binwrap3/sub2/file1\.txt,v
done
Checking in file1\.txt;
${CVSROOT_DIRNAME}/binwrap3/sub2/file1\.txt,v  <--  file1\.txt
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

          dotest binwrap3-top5 "grep foo-t.sb ./CVS/Entries" \
                 "/foo-t.sb/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-top6 "grep bar-t.c0 ./CVS/Entries" \
                 "/bar-t.c0/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub1-1 "grep foo-b.c1 sub1/CVS/Entries" \
                 "/foo-b.c1/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-sub1-2 "grep foo-b.sb sub1/CVS/Entries" \
                 "/foo-b.sb/1.1.1.1/[A-Za-z0-9 	:]*/-kb/"

          dotest binwrap3-sub1-3 "grep foo-t.c0 sub1/CVS/Entries" \
                 "/foo-t.c0/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub1-4 "grep foo-t.st sub1/CVS/Entries" \
                 "/foo-t.st/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub1-5 "grep foo-t.sb sub1/CVS/Entries" \
                 "/foo-t.sb/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub1-6 "grep bar-t.c1 sub1/CVS/Entries" \
                 "/bar-t.c1/1.1.1.1/[A-Za-z0-9 	:]*//"

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

          dotest binwrap3-sub2-7 "grep foo-t.sb sub2/CVS/Entries" \
                 "/foo-t.sb/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-sub2-8 "grep bar-t.st sub2/CVS/Entries" \
                 "/bar-t.st/1.1.1.1/[A-Za-z0-9 	:]*//"

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

          dotest binwrap3-subsub7 "grep foo-t.sb sub2/subsub/CVS/Entries" \
                 "/foo-t.sb/1.1.1.1/[A-Za-z0-9 	:]*//"

          dotest binwrap3-subsub8 "grep bar-t.c3 sub2/subsub/CVS/Entries" \
                 "/bar-t.c3/1.1.1.1/[A-Za-z0-9 	:]*//"

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
          CVSWRAPPERS=${CVSWRAPPERS_save}
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
${CVSROOT_DIRNAME}/CVSROOT/cvswrappers,v  <--  cvswrappers
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..
	  mkdir m1; cd m1
	  dotest mwrap-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest mwrap-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  dotest mwrap-3 "${testcvs} add aa" \
"${PROG} add: scheduling file .aa. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest mwrap-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aa,v
done
Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  cd ../..
	  mkdir m2; cd m2
	  dotest mwrap-5 "${testcvs} -q co first-dir" "U first-dir/aa"
	  cd first-dir
	  echo "changed in m2" >aa
	  dotest mwrap-6 "${testcvs} -q ci -m m2-mod" \
"Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../..
	  cd m1/first-dir
	  echo "changed in m1" >aa
	  dotest mwrap-7 "$testcvs -nq update" \
"${PROG} update: nonmergeable file needs merge
${PROG} update: revision 1\.2 from repository is now in aa
${PROG} update: file from working directory is now in \.#aa\.1\.1
C aa"
	  dotest mwrap-8 "${testcvs} -q update" \
"$PROG update: nonmergeable file needs merge
${PROG} update: revision 1\.2 from repository is now in aa
${PROG} update: file from working directory is now in \.#aa\.1\.1
C aa"
	  dotest mwrap-9 "cat aa" "changed in m2"
	  dotest mwrap-10 "cat .#aa.1.1" "changed in m1"
	  cd ../..
	  cd CVSROOT
	  echo '# comment out' >cvswrappers
	  dotest mwrap-ce "${testcvs} -q ci -m wrapper-mod" \
"Checking in cvswrappers;
${CVSROOT_DIRNAME}/CVSROOT/cvswrappers,v  <--  cvswrappers
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
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
	  # taginfo: taginfo
	  # config: config

	  # On Windows, we can't check out CVSROOT, because the case
	  # insensitivity means that this conflicts with cvsroot.
	  mkdir wnt
	  cd wnt

	  dotest info-1 "${testcvs} -q co CVSROOT" "[UP] CVSROOT${DOTSTAR}"
	  cd CVSROOT
	  rm -f $TESTDIR/testlog $TESTDIR/testlog2
	  echo "ALL sh -c \"echo x\${=MYENV}\${=OTHER}y\${=ZEE}=\$USER=\$CVSROOT= >>$TESTDIR/testlog; cat >/dev/null\"" > loginfo
          # The following cases test the format string substitution
          echo "ALL echo %{sVv} >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %{v} >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %s >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "ALL echo %{V}AX >>$TESTDIR/testlog2; cat >/dev/null" >> loginfo
          echo "first-dir echo %sux >>$TESTDIR/testlog2; cat >/dev/null" \
            >> loginfo

	  # Might be nice to move this to crerepos tests; it should
	  # work to create a loginfo file if you didn't create one
	  # with "cvs init".
	  : dotest info-2 "${testcvs} add loginfo" \
"${PROG}"' add: scheduling file `loginfo'"'"' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  dotest info-3 "${testcvs} -q ci -m new-loginfo" \
"Checking in loginfo;
${CVSROOT_DIRNAME}/CVSROOT/loginfo,v  <--  loginfo
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..

	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest info-5 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  touch file1
	  dotest info-6 "${testcvs} add file1" \
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  echo "cvs -s OTHER=not-this -s MYENV=env-" >>$HOME/.cvsrc
	  dotest info-6a "${testcvs} -q -s OTHER=value ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
${PROG} commit: loginfo:1: no such user variable \${=ZEE}"
	  echo line0 >>file1
	  dotest info-6b "${testcvs} -q -sOTHER=foo ci -m mod-it" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
${PROG} commit: loginfo:1: no such user variable \${=ZEE}"
	  echo line1 >>file1
	  dotest info-7 "${testcvs} -q -s OTHER=value -s ZEE=z ci -m mod-it" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  cd ..
	  dotest info-9 "cat $TESTDIR/testlog" "xenv-valueyz=${username}=${CVSROOT_DIRNAME}="
          dotest info-10 "cat $TESTDIR/testlog2" \
'first-dir file1,NONE,1.1
first-dir 1.1
first-dir file1
first-dir NONEAX
first-dir file1ux
first-dir file1,1.1,1.2
first-dir 1.2
first-dir file1
first-dir 1.1AX
first-dir file1ux
first-dir file1,1.2,1.3
first-dir 1.3
first-dir file1
first-dir 1.2AX
first-dir file1ux'

	  cd CVSROOT
	  echo '# do nothing' >loginfo
	  dotest info-11 "${testcvs} -q -s ZEE=garbage ci -m nuke-loginfo" \
"Checking in loginfo;
${CVSROOT_DIRNAME}/CVSROOT/loginfo,v  <--  loginfo
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  # Now test verifymsg
	  cat >${TESTDIR}/vscript <<EOF
#!${TESTSHELL}
if sed 1q < \$1 | grep '^BugId:[ ]*[0-9][0-9]*$' > /dev/null; then
    exit 0
elif sed 1q < \$1 | grep '^BugId:[ ]*new$' > /dev/null; then
    echo A new bugid was found. >> \$1
    exit 0
else
    echo "No BugId found."
    sleep 1
    exit 1
fi
EOF
	  cat >${TESTDIR}/vscript2 <<EOF
#!${TESTSHELL}
if test -f CVS/Repository; then
	repo=\`cat CVS/Repository\`
else
	repo=\`pwd\`
fi
echo \$repo
if echo "\$repo" |grep yet-another/ >/dev/null 2>&1; then
	exit 1
else
	exit 0
fi
EOF
	  # Grumble, grumble, mumble, search for "Cygwin".
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod +x ${TESTDIR}/vscript*"
	  else
	    chmod +x ${TESTDIR}/vscript*
	  fi
	  echo "^first-dir/yet-another\\(/\\|\$\\) ${TESTDIR}/vscript2" >>verifymsg
	  echo "^first-dir\\(/\\|\$\\) ${TESTDIR}/vscript" >>verifymsg
	  echo "^missing-script\$ ${TESTDIR}/bogus" >>verifymsg
	  echo "^missing-var\$ ${TESTDIR}/vscript \${=Bogus}" >>verifymsg
	  # first test the directory independant verifymsg
	  dotest info-v1 "${testcvs} -q ci -m add-verification" \
"Checking in verifymsg;
${CVSROOT_DIRNAME}/CVSROOT/verifymsg,v  <--  verifymsg
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  cd ../first-dir
	  echo line2 >>file1
	  dotest_fail info-v2 "${testcvs} -q ci -m bogus" \
"No BugId found\.
${PROG} \[commit aborted\]: Message verification failed"

	  cat >${TESTDIR}/comment.tmp <<EOF
BugId: 42
and many more lines after it
EOF
	  dotest info-v3 "${testcvs} -q ci -F ${TESTDIR}/comment.tmp" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"
	  rm ${TESTDIR}/comment.tmp

	  cd ..
	  mkdir another-dir
	  cd another-dir
	  touch file2
	  dotest_fail info-v4 \
	    "${testcvs} import -m bogus first-dir/another x y" \
"No BugId found\.
${PROG} \[import aborted\]: Message verification failed"

	  # now verify that directory dependent verifymsgs work
	  dotest info-v5 \
	    "${testcvs} import -m bogus first-dir/yet-another x y" \
"${TESTDIR}/wnt/another-dir
N first-dir/yet-another/file2

No conflicts created by this import" \
"${CVSROOT_DIRNAME}/first-dir/yet-another
N first-dir/yet-another/file2

No conflicts created by this import"

	  # FIXMECVS
	  #
	  # note that in the local case the error message is the same as
	  # info-v5
	  #
	  # This means that the verifymsg scripts cannot reliably and
	  # consistantly obtain information on which directory is being
	  # committed to.  Thus it is currently useless for them to be
	  # running in every dir.  They should either be run once or
	  # directory information should be passed.
	  if $remote; then
	    dotest_fail info-v6r \
	      "${testcvs} import -m bogus first-dir/yet-another/and-another x y" \
"${CVSROOT_DIRNAME}/first-dir/yet-another/and-another
${PROG} \[import aborted\]: Message verification failed"
	  else
	    dotest info-v6 \
	      "${testcvs} import -m bogus first-dir/yet-another/and-another x y" \
"${TESTDIR}/wnt/another-dir
N first-dir/yet-another/and-another/file2

No conflicts created by this import"
	  fi

	  # check that errors invoking the script cause verification failure
	  #
	  # The second text below occurs on Cygwin, where I assume execvp
	  # does not return to let CVS print the error message when its
	  # argument does not exist.
	  dotest_fail info-v7 "${testcvs} import -m bogus missing-script x y" \
"${PROG} import: cannot exec ${TESTDIR}/bogus: No such file or directory
${PROG} \[import aborted\]: Message verification failed" \
"${PROG} \[import aborted\]: Message verification failed"

	  dotest_fail info-v8 "${testcvs} import -m bogus missing-var x y" \
"${PROG} import: verifymsg:25: no such user variable \${=Bogus}
${PROG} \[import aborted\]: Message verification failed"

	  rm file2
	  cd ..
	  rmdir another-dir

	  cd CVSROOT
	  echo "RereadLogAfterVerify=always" >>config
	  dotest info-rereadlog-1 "${testcvs} -q ci -m add-RereadLogAfterVerify=always" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../first-dir
	  echo line3 >>file1
	  cat >${TESTDIR}/comment.tmp <<EOF
BugId: new
See what happens next.
EOF
	  dotest info-reread-2 "${testcvs} -q ci -F ${TESTDIR}/comment.tmp" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest info-reread-3 "${testcvs} -q log -N -r1.5 file1" "
.*
BugId: new
See what happens next.
A new bugid was found.
============================================================================="

	  cd ../CVSROOT
	  grep -v "RereadLogAfterVerify" config > config.new
	  mv config.new config
	  echo "RereadLogAfterVerify=stat" >>config
	  dotest info-reread-4 "${testcvs} -q ci -m add-RereadLogAfterVerify=stat" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../first-dir
	  echo line4 >>file1
	  cat >${TESTDIR}/comment.tmp <<EOF
BugId: new
See what happens next with stat.
EOF
	  dotest info-reread-5 "${testcvs} -q ci -F ${TESTDIR}/comment.tmp" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.6; previous revision: 1\.5
done"
	  dotest info-reread-6 "${testcvs} -q log -N -r1.6 file1" "
.*
BugId: new
See what happens next with stat.
A new bugid was found.
============================================================================="

	  cd ../CVSROOT
	  grep -v "RereadLogAfterVerify" config > config.new
	  mv config.new config
	  echo "RereadLogAfterVerify=never" >>config
	  dotest info-reread-7 "${testcvs} -q ci -m add-RereadLogAfterVerify=never" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../first-dir
	  echo line5 >>file1
	  cat >${TESTDIR}/comment.tmp <<EOF
BugId: new
See what happens next.
EOF
	  dotest info-reread-8 "${testcvs} -q ci -F ${TESTDIR}/comment.tmp" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.7; previous revision: 1\.6
done"
	  dotest info-reread-6 "${testcvs} -q log -N -r1.7 file1" "
.*
BugId: new
See what happens next.
============================================================================="

	  cd ../CVSROOT
	  echo 'DEFAULT false' >verifymsg
	  echo 'DEFAULT true' >>verifymsg
	  echo '# defaults' >config
	  dotest info-multdef "${testcvs} -q ci -m multdef" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
Checking in verifymsg;
${CVSROOT_DIRNAME}/CVSROOT/verifymsg,v  <--  verifymsg
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"

	  cd ../CVSROOT
	  echo '# do nothing' >verifymsg
	  dotest info-cleanup-verifymsg "${testcvs} -q ci -m nuke-verifymsg" \
"${PROG} commit: Multiple .DEFAULT. lines (1 and 2) in verifymsg file
Checking in verifymsg;
${CVSROOT_DIRNAME}/CVSROOT/verifymsg,v  <--  verifymsg
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  rm ${TESTDIR}/vscript*
	  cd ..

	  dotest_fail info-cleanup-0 "${testcvs} -n release -d CVSROOT" \
"${PROG} \[release aborted\]: cannot run command ${DOTSTAR}"

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
	  rm $HOME/.cvsrc
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	taginfo)
	  # Tests of the CVSROOT/taginfo file.  See the comment at the
	  # "info" tests for a full list of administrative file tests.

	  # Tests to add:
	  #   -F to move

	  mkdir 1; cd 1
	  dotest taginfo-1 "${testcvs} -q co CVSROOT" "U CVSROOT/${DOTSTAR}"
	  cd CVSROOT
	  cat >${TESTDIR}/1/loggit <<EOF
#!${TESTSHELL}
if test "\$1" = rejectme; then
  exit 1
else
  echo "\$@" >>${TESTDIR}/1/taglog
  exit 0
fi
EOF
	  # #^@&!^@ Cygwin.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod +x ${TESTDIR}/1/loggit"
	  else
	    chmod +x ${TESTDIR}/1/loggit
	  fi
	  echo "ALL ${TESTDIR}/1/loggit" >taginfo
	  dotest taginfo-2 "${testcvs} -q ci -m check-in-taginfo" \
"Checking in taginfo;
${CVSROOT_DIRNAME}/CVSROOT/taginfo,v  <--  taginfo
new revision: 1\.2; previous revision: 1\.1
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..

	  # taginfo-3 used to rely on the top-level CVS directory
	  # being created to add "first-dir" to the repository.  Since
	  # that won't happen anymore, we create the directory in the
	  # repository.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest taginfo-3 "${testcvs} -q co first-dir" ''

	  cd first-dir
	  echo first >file1
	  dotest taginfo-4 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest taginfo-5 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest taginfo-6 "${testcvs} -q tag tag1" "T file1"
	  dotest taginfo-7 "${testcvs} -q tag -b br" "T file1"
	  dotest taginfo-8 "$testcvs -q update -r br" '[UP] file1'
	  echo add text on branch >>file1
	  dotest taginfo-9 "${testcvs} -q ci -m modify-on-br" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest taginfo-10 "${testcvs} -q tag -F -c brtag" "T file1"

	  dotest_fail taginfo-11 "${testcvs} -q tag rejectme" \
"${PROG} tag: Pre-tag check failed
${PROG} \[tag aborted\]: correct the above errors first!"

	  # When we are using taginfo to allow/disallow, it would be
	  # convenient to be able to use "cvs -n tag" to test whether
	  # the allow/disallow functionality is working as expected.
	  dotest taginfo-12 "${testcvs} -nq tag rejectme" "T file1"

	  # But when taginfo is used for logging, it is a pain for -n
	  # to call taginfo, since taginfo doesn't know whether -n was
	  # specified or not.
	  dotest taginfo-13 "${testcvs} -nq tag would-be-tag" "T file1"

	  # Deleting: the cases are basically either the tag existed,
	  # or it didn't exist.
	  dotest taginfo-14 "${testcvs} -q tag -d tag1" "D file1"
	  dotest taginfo-15 "${testcvs} -q tag -d tag1" ""

	  # Likewise with rtag.
	  dotest taginfo-16 "${testcvs} -q rtag tag1 first-dir" ""
	  dotest taginfo-17 "${testcvs} -q rtag -d tag1 first-dir" ""
	  dotest taginfo-18 "${testcvs} -q rtag -d tag1 first-dir" ""

	  # The "br" example should be passing 1.1.2 or 1.1.0.2.
	  # But it turns out that is very hard to implement, since
	  # check_fileproc doesn't know what branch number it will
	  # get.  Probably the whole thing should be re-architected
	  # so that taginfo only allows/denies tagging, and a new
	  # hook, which is done from tag_fileproc, does logging.
	  # That would solve this, some more subtle races, and also
	  # the fact that it is nice for users to run "-n tag foo" to
	  # see whether a tag would be allowed.  Failing that,
	  # I suppose passing "1.1.branch" or "branch" for "br"
	  # would be an improvement.
	  dotest taginfo-examine "cat ${TESTDIR}/1/taglog" \
"tag1 add ${CVSROOT_DIRNAME}/first-dir file1 1.1
br add ${CVSROOT_DIRNAME}/first-dir file1 1.1
brtag mov ${CVSROOT_DIRNAME}/first-dir file1 1.1.2.1
tag1 del ${CVSROOT_DIRNAME}/first-dir file1 1.1
tag1 del ${CVSROOT_DIRNAME}/first-dir
tag1 add ${CVSROOT_DIRNAME}/first-dir file1 1.1
tag1 del ${CVSROOT_DIRNAME}/first-dir file1 1.1
tag1 del ${CVSROOT_DIRNAME}/first-dir"

	  cd ..
	  cd CVSROOT
	  echo '# Keep life simple' > taginfo
	  dotest taginfo-cleanup-1 "${testcvs} -q ci -m check-in-taginfo" \
"Checking in taginfo;
${CVSROOT_DIRNAME}/CVSROOT/taginfo,v  <--  taginfo
new revision: 1\.3; previous revision: 1\.2
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..
	  cd ..
	  rm -r 1
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
	  # We can't rely on specific revisions, since other tests
	  # might need to modify CVSROOT/config
	  dotest config-3 "${testcvs} -q ci -m change-to-bogus-line" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  echo 'BogusOption=yes' >config
	  dotest config-4 "${testcvs} -q ci -m change-to-bogus-opt" \
"${PROG} [a-z]*: syntax error in ${CVSROOT_DIRNAME}/CVSROOT/config: line 'bogus line' is missing '='
Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  echo '# No config is a good config' > config
	  dotest config-5 "${testcvs} -q ci -m change-to-comment" \
"${PROG} [a-z]*: ${CVSROOT_DIRNAME}/CVSROOT/config: unrecognized keyword 'BogusOption'
Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  dotest config-6 "${testcvs} -q update" ''
	  echo 'IgnoreUnknownConfigKeys=yes' > config
	  echo 'BogusOption=yes' >> config
	  dotest config-7 "${testcvs} -q ci -m change-to-comment" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  dotest config-8 "${testcvs} -q update" ''
	  echo '# No config is a good config' > config
	  dotest config-9 "${testcvs} -q ci -m change-to-comment" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  dotest config-10 "${testcvs} -q update" ''

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
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'

	  dotest serverpatch-3 "${testcvs} -q commit -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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

	  # Remove the tag.  Prior to 1.11.23, this left the tag string in the
	  # expansion of the Name keyword.
	  dotest serverpatch-6 "$testcvs -q update -A first-dir" \
'U first-dir/file1'

	  # Modify and check in the first copy.
	  cd ../1/first-dir
	  echo '2' >> file1
	  dotest serverpatch-7 "${testcvs} -q ci -mx file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"

	  # Now update the second copy.  Prior to 1.11.23, the patch would fail
	  # using remote CVS, forcing the file to be refetched.
	  cd ../../2/first-dir
	  dotest serverpatch-8 "${testcvs} -q update" \
'[UP] file1'

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
	  #   -d: logopt, rcs
	  #   -s: logopt, rcs3
	  #   -R: logopt, rcs3
	  #   -w, -t: not tested yet (TODO)

	  # Check in a file with a few revisions and branches.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  dotest log-1 "${testcvs} -q co first-dir" ''
	  cd first-dir
	  echo 'first revision' > file1
	  echo 'first revision' > file2
	  dotest log-2 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"

	  # While we're at it, check multi-line comments, input from file,
	  # and trailing whitespace trimming
	  echo 'line 1     '	 >${TESTDIR}/comment.tmp
	  echo '     '		>>${TESTDIR}/comment.tmp
	  echo 'line 2	'	>>${TESTDIR}/comment.tmp
	  echo '	'	>>${TESTDIR}/comment.tmp
	  echo '  	  '	>>${TESTDIR}/comment.tmp
	  dotest log-3 "${testcvs} -q commit -F ${TESTDIR}/comment.tmp" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  rm -f ${TESTDIR}/comment.tmp

	  echo 'second revision' > file1
	  echo 'second revision' > file2
	  dotest log-4 "${testcvs} -q ci -m2 file1 file2" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.2; previous revision: 1\.1
done"

	  dotest log-5 "${testcvs} -q tag -b branch file1" 'T file1'
	  dotest log-5a "${testcvs} -q tag tag1 file2" 'T file2'

	  echo 'third revision' > file1
	  echo 'third revision' > file2
	  dotest log-6 "${testcvs} -q ci -m3 file1 file2" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.3; previous revision: 1\.2
done"

	  dotest log-6a "${testcvs} -q tag tag2 file2" 'T file2'

	  dotest log-7 "${testcvs} -q update -r branch" \
"[UP] file1
${PROG} update: file2 is no longer in the repository"

	  echo 'first branch revision' > file1
	  dotest log-8 "${testcvs} -q ci -m1b file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"

	  dotest log-9 "${testcvs} -q tag tag file1" 'T file1'

	  echo 'second branch revision' > file1
	  dotest log-10 "${testcvs} -q ci -m2b file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done"

	  # Set up a bunch of shell variables to make the later tests
	  # easier to describe.=
	  log_header1="
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.3
branch:
locks: strict
access list:"
	  rlog_header1="
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
head: 1\.3
branch:
locks: strict
access list:"
	  log_tags1='symbolic names:
	tag: 1\.2\.2\.1
	branch: 1\.2\.0\.2'
	  log_keyword='keyword substitution: kv'
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
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-12 "${testcvs} log -N file1" \
"${log_header1}
${log_keyword}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-13 "${testcvs} log -b file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 3
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-14 "${testcvs} log -r file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-14a "${testcvs} log -rHEAD file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  # The user might not realize that "-r" must not take a space.
	  # In the error message, HEAD is a file name, not a tag name (which
	  # might be confusing itself).
	  dotest_fail log-14b "${testcvs} log -r HEAD file1" \
"${PROG} log: nothing known about HEAD
${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

#	  Check that unusual syntax works correctly.

	  dotest log-14c "${testcvs} log -r: file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"
	  dotest log-14d "${testcvs} log -r, file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"
	  dotest log-14e "${testcvs} log -r. file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"
	  dotest log-14f "${testcvs} log -r:: file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 0
description:
${log_trailer}"

	  dotest log-15 "${testcvs} log -r1.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-16 "${testcvs} log -r1.2.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  # This test would fail with the old invocation of rlog, but it
	  # works with the builtin log support.
	  dotest log-17 "${testcvs} log -rbranch file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-18 "${testcvs} log -r1.2.2. file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  # Multiple -r options are undocumented; see comments in
	  # cvs.texinfo about whether they should be deprecated.
	  dotest log-18a "${testcvs} log -r1.2.2.2 -r1.3:1.3 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2b}
${log_trailer}"

	  # This test would fail with the old invocation of rlog, but it
	  # works with the builtin log support.
	  dotest log-19 "${testcvs} log -rbranch. file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  dotest log-20 "${testcvs} log -r1.2: file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"

	  dotest log-20a "${testcvs} log -r1.2:: file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-21 "${testcvs} log -r:1.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-21a "${testcvs} log -r::1.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-22 "${testcvs} log -r1.1:1.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-22a "${testcvs} log -r1.1::1.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-22b "${testcvs} log -r1.1::1.3 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"

	  # Test BASE pseudotag
	  dotest log-23 "${testcvs} log -rBASE file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  dotest log-24 "${testcvs} -q up -r1.2 file1" "[UP] file1"
	  dotest log-25 "${testcvs} log -rBASE file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-26 "${testcvs} -q up -rbranch file1" "[UP] file1"

	  # Now the same tests but with rlog

	  dotest log-r11 "${testcvs} rlog first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-r12 "${testcvs} rlog -N first-dir/file1" \
"${rlog_header1}
${log_keyword}
total revisions: 5;	selected revisions: 5
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-r13 "${testcvs} rlog -b first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 3
description:
${log_rev3}
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-r14 "${testcvs} rlog -r first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-r14a "${testcvs} rlog -rHEAD first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest_fail log-r14b "${testcvs} rlog -r HEAD first-dir/file1" \
"${PROG} rlog: cannot find module .HEAD. - ignored
${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-r14c "${testcvs} rlog -r: first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"
	  dotest log-r14d "${testcvs} rlog -r, first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"
	  dotest log-r14e "${testcvs} rlog -r. first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"
	  dotest log-r14f "${testcvs} rlog -r:: first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 0
description:
${log_trailer}"

	  dotest log-r15 "${testcvs} rlog -r1.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-r16 "${testcvs} rlog -r1.2.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-r17 "${testcvs} rlog -rbranch first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"

	  dotest log-r18 "${testcvs} rlog -r1.2.2. first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  dotest log-r18a "${testcvs} rlog -r1.2.2.2 -r1.3:1.3 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2b}
${log_trailer}"

	  dotest log-r19 "${testcvs} rlog -rbranch. first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2b}
${log_trailer}"

	  dotest log-r20 "${testcvs} rlog -r1.2: first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"

	  dotest log-r20a "${testcvs} rlog -r1.2:: first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev3}
${log_trailer}"

	  dotest log-r21 "${testcvs} rlog -r:1.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-r21a "${testcvs} rlog -r::1.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-r22 "${testcvs} rlog -r1.1:1.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev2}
${log_rev1}
${log_trailer}"

	  dotest log-r22a "${testcvs} rlog -r1.1::1.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 1
description:
${log_rev2}
${log_trailer}"

	  dotest log-r22b "${testcvs} rlog -r1.1::1.3 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"

	  # Test BASE pseudotag
	  dotest log-r23 "${testcvs} rlog -rBASE first-dir/file1" \
"${PROG} rlog: warning: no revision .BASE. in .${CVSROOT_DIRNAME}/first-dir/file1,v.
${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 0
description:
${log_trailer}"

	  dotest log-r24 "${testcvs} -q up -r1.2 file1" "[UP] file1"
	  dotest log-r25 "${testcvs} rlog -rBASE first-dir/file1" \
"${PROG} rlog: warning: no revision .BASE. in .${CVSROOT_DIRNAME}/first-dir/file1,v.
${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 0
description:
${log_trailer}"

	  # Test when head is dead

	  dotest log-d0 "${testcvs} -q up -A" \
"[UP] file1
U file2"
	  dotest log-d1 "${testcvs} -q rm -f file1" \
"${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  dotest log-d2 "${testcvs} -q ci -m4" \
"Removing file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: delete; previous revision: 1\.3
done"

	  log_header1="
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
Working file: file1
head: 1\.4
branch:
locks: strict
access list:"
	  rlog_header1="
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
head: 1\.4
branch:
locks: strict
access list:"
	  log_header2="
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
Working file: file2
head: 1\.3
branch:
locks: strict
access list:"
	  rlog_header2="
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
head: 1\.3
branch:
locks: strict
access list:"
	  log_tags2='symbolic names:
	tag2: 1\.3
	tag1: 1\.2'
	  log_rev4="${log_dash} 1\.4
date: [0-9/]* [0-9:]*;  author: ${username};  state: dead;  lines: ${PLUS}0 -0
4"
	  log_rev22="${log_dash} 1\.2
${log_date}${log_lines}
2"

	  dotest log-d3 "${testcvs} log -rbranch file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"
	  dotest log-rd3 "${testcvs} rlog -rbranch first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"
	  dotest log-d4 "${testcvs} -q log -rbranch" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}
${PROG} log: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v.
${log_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 0
description:
${log_trailer}"
	  dotest log-d4a "${testcvs} -q log -t -rbranch" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6
description:
${log_trailer}
${PROG} log: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v.
${log_header2}
${log_tags2}
${log_keyword}
total revisions: 3
description:
${log_trailer}"
	  dotest log-d4b "${testcvs} -q log -tS -rbranch" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_trailer}
${PROG} log: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v."
	  dotest log-d4c "${testcvs} -q log -h -rbranch" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6
${log_trailer}
${PROG} log: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v.
${log_header2}
${log_tags2}
${log_keyword}
total revisions: 3
${log_trailer}"
	  dotest log-d4d "${testcvs} -q log -hS -rbranch" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
${log_trailer}
${PROG} log: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v."
	  dotest log-d4e "${testcvs} -q log -R -rbranch" \
"${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
${CVSROOT_DIRNAME}/first-dir/file2,v"
	  dotest log-d4f "${testcvs} -q log -R -S -rbranch" \
"${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
${PROG} log: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v."
	  dotest log-rd4 "${testcvs} -q rlog -rbranch first-dir" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}
${PROG} rlog: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v.
${rlog_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 0
description:
${log_trailer}"
	  dotest log-rd4a "${testcvs} -q rlog -t -rbranch first-dir" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6
description:
${log_trailer}
${PROG} rlog: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v.
${rlog_header2}
${log_tags2}
${log_keyword}
total revisions: 3
description:
${log_trailer}"
	  dotest log-rd4b "${testcvs} -q rlog -St -rbranch first-dir" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_trailer}
${PROG} rlog: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v."
	  dotest log-rd4c "${testcvs} -q rlog -h -rbranch first-dir" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6
${log_trailer}
${PROG} rlog: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v.
${rlog_header2}
${log_tags2}
${log_keyword}
total revisions: 3
${log_trailer}"
	  dotest log-rd4d "${testcvs} -q rlog -Sh -rbranch first-dir" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
${log_trailer}
${PROG} rlog: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v."
	  dotest log-rd4e "${testcvs} -q rlog -R -rbranch first-dir" \
"${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
${CVSROOT_DIRNAME}/first-dir/file2,v"
	  dotest log-rd4f "${testcvs} -q rlog -R -S -rbranch first-dir" \
"${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
${PROG} rlog: warning: no revision .branch. in .${CVSROOT_DIRNAME}/first-dir/file2,v."
	  dotest log-d5 "${testcvs} log -r1.2.2.1:1.2.2.2 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"
	  dotest log-rd5 "${testcvs} rlog -r1.2.2.1:1.2.2.2 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}"
	  dotest log-d6 "${testcvs} -q log -r1.2.2.1:1.2.2.2" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}
${log_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 0
description:
${log_trailer}"
	  dotest log-rd6 "${testcvs} -q rlog -r1.2.2.1:1.2.2.2 first-dir" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev2b}
${log_rev1b}
${log_trailer}
${rlog_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 0
description:
${log_trailer}"
	  dotest log-d7 "${testcvs} log -r1.2:1.3 file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"
	  dotest log-rd7 "${testcvs} -q rlog -r1.2:1.3 first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 2
description:
${log_rev3}
${log_rev2}
${log_trailer}"
	  dotest log-d8 "${testcvs} -q log -rtag1:tag2" \
"${PROG} log: warning: no revision .tag1. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${PROG} log: warning: no revision .tag2. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${log_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 0
description:
${log_trailer}
${log_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 2
description:
${log_rev3}
${log_rev22}
${log_trailer}"
	  dotest log-d8a "${testcvs} -q log -rtag1:tag2 -S" \
"${PROG} log: warning: no revision .tag1. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${PROG} log: warning: no revision .tag2. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${log_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 2
description:
${log_rev3}
${log_rev22}
${log_trailer}"
	  dotest log-rd8 "${testcvs} -q rlog -rtag1:tag2 first-dir" \
"${PROG} rlog: warning: no revision .tag1. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${PROG} rlog: warning: no revision .tag2. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 6;	selected revisions: 0
description:
${log_trailer}
${rlog_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 2
description:
${log_rev3}
${log_rev22}
${log_trailer}"
	  dotest log-rd8a "${testcvs} -q rlog -rtag1:tag2 -S first-dir" \
"${PROG} rlog: warning: no revision .tag1. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${PROG} rlog: warning: no revision .tag2. in .${CVSROOT_DIRNAME}/first-dir/Attic/file1,v.
${rlog_header2}
${log_tags2}
${log_keyword}
total revisions: 3;	selected revisions: 2
description:
${log_rev3}
${log_rev22}
${log_trailer}"

	  dotest log-d99 "${testcvs} -q up -rbranch" \
"[UP] file1
${PROG} update: file2 is no longer in the repository"

	  # Now test outdating revisions

	  dotest log-o0 "${testcvs} admin -o 1.2.2.2:: file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
done"
	  dotest log-o1 "${testcvs} admin -o ::1.2.2.1 file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
done"
	  dotest log-o2 "${testcvs} admin -o 1.2.2.1:: file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file1,v
deleting revision 1\.2\.2\.2
done"
	  dotest log-o3 "${testcvs} log file1" \
"${log_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 5
description:
${log_rev4}
${log_rev3}
${log_rev2}
${log_rev1}
${log_rev1b}
${log_trailer}"
	  dotest log-ro3 "${testcvs} rlog first-dir/file1" \
"${rlog_header1}
${log_tags1}
${log_keyword}
total revisions: 5;	selected revisions: 5
description:
${log_rev4}
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
"${PROG}"' add: scheduling file `file1'\'' for addition
'"${PROG}"' add: use .'"${PROG}"' commit. to add this file permanently'
	  dotest log2-3 "${testcvs} -q commit -m 1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  # Setting the file description with add -m doesn't yet work
	  # client/server, so skip log2-4 for remote.
	  if $remote; then :; else

	    dotest log2-4 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest log2-6 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  echo 'longer description' >${TESTDIR}/descrip
	  echo 'with two lines' >>${TESTDIR}/descrip
	  dotest log2-7 "${testcvs} admin -t${TESTDIR}/descrip file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest_fail log2-7a "${testcvs} admin -t${TESTDIR}/nonexist file1" \
"${PROG} \[[a-z]* aborted\]: can't stat ${TESTDIR}/nonexist: No such file or directory"
	  dotest log2-8 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  # TODO: `cvs admin -t "my message" file1' is a request to
	  # read the message from stdin and to operate on two files.
	  # Should test that there is an error because "my message"
	  # doesn't exist.

	  dotest log2-9 "echo change from stdin | ${testcvs} admin -t -q file1" ""
	  dotest log2-10 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  cd ..
	  rm ${TESTDIR}/descrip
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir

	  ;;

	logopt)
	  # Some tests of log.c's option parsing and such things.
	  mkdir 1; cd 1
	  dotest logopt-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest logopt-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo hi >file1
	  dotest logopt-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest logopt-4 "${testcvs} -q ci -m add file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ..

	  dotest logopt-5 "${testcvs} log -R -d 2038-01-01" \
"${PROG} log: Logging \.
${PROG} log: Logging first-dir
${CVSROOT_DIRNAME}/first-dir/file1,v"
	  dotest logopt-6 "${testcvs} log -d 2038-01-01 -R" \
"${PROG} log: Logging \.
${PROG} log: Logging first-dir
${CVSROOT_DIRNAME}/first-dir/file1,v"
	  dotest logopt-6a "${testcvs} log -Rd 2038-01-01" \
"${PROG} log: Logging \.
${PROG} log: Logging first-dir
${CVSROOT_DIRNAME}/first-dir/file1,v"
	  dotest logopt-7 "${testcvs} log -s Exp -R" \
"${PROG} log: Logging \.
${PROG} log: Logging first-dir
${CVSROOT_DIRNAME}/first-dir/file1,v"

	  cd ..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	ann)
	  # Tests of "cvs annotate".  See also:
	  #   basica-10  A simple annotate test
	  #   rcs        Annotate and the year 2000
	  #   keywordlog Annotate and $Log.
	  mkdir 1; cd 1
	  dotest ann-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest ann-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  cat >file1 <<EOF
this
is
the
ancestral
file
EOF
	  dotest ann-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest ann-4 "${testcvs} -q ci -m add file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"
	  # Note that this annotates the trunk despite the presence
	  # of a sticky tag in the current directory.  This is
	  # fairly bogus, but it is the longstanding behavior for
	  # whatever that is worth.
	  dotest ann-10 "${testcvs} ann" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.3          ($username8 *[0-9a-zA-Z-]*): trunk file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line"
	  dotest ann-10blame "${testcvs} blame" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.3          ($username8 *[0-9a-zA-Z-]*): trunk file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line"
	  dotest ann-11 "${testcvs} ann -r br" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.1          ($username8 *[0-9a-zA-Z-]*): file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): and some
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): branched content"
	  # FIXCVS: shouldn't "-r 1.2.0.2" be the same as "-r br"?
	  dotest ann-12 "${testcvs} ann -r 1.2.0.2 file1" ""
	  dotest ann-13 "${testcvs} ann -r 1.2.2 file1" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.1          ($username8 *[0-9a-zA-Z-]*): file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): and some
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): branched content"
	  dotest_fail ann-14 "${testcvs} ann -r bill-clintons-chastity file1" \
"${PROG} \[annotate aborted\]: no such tag bill-clintons-chastity"

	  # Now get rid of the working directory and test rannotate

	  cd ../..
	  rm -r 1
	  dotest ann-r10 "${testcvs} rann first-dir" \
"
Annotations for first-dir/file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.3          ($username8 *[0-9a-zA-Z-]*): trunk file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line"
	  dotest ann-r11 "${testcvs} rann -r br first-dir" \
"
Annotations for first-dir/file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.1          ($username8 *[0-9a-zA-Z-]*): file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): and some
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): branched content"
	  dotest ann-r12 "${testcvs} rann -r 1.2.0.2 first-dir/file1" ""
	  dotest ann-r13 "${testcvs} rann -r 1.2.2 first-dir/file1" \
"
Annotations for first-dir/file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          ($username8 *[0-9a-zA-Z-]*): this
1\.1          ($username8 *[0-9a-zA-Z-]*): is
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.1          ($username8 *[0-9a-zA-Z-]*): file
1\.2          ($username8 *[0-9a-zA-Z-]*): 
1\.2          ($username8 *[0-9a-zA-Z-]*): with
1\.2          ($username8 *[0-9a-zA-Z-]*): a
1\.2          ($username8 *[0-9a-zA-Z-]*): blank
1\.2          ($username8 *[0-9a-zA-Z-]*): line
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): and some
1\.2\.2\.1      ($username8 *[0-9a-zA-Z-]*): branched content"
	  dotest_fail ann-r14 "${testcvs} rann -r bill-clintons-chastity first-dir/file1" \
"${PROG} \[rannotate aborted\]: no such tag bill-clintons-chastity"

	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	ann-id)
	  # Demonstrate that cvs-1.9.28.1 improperly expands rcs keywords in
	  # the output of `cvs annotate' -- it uses values from the previous
	  # delta.  In this case, `1.1' instead of `1.2', even though it puts
	  # the proper version number on the prefix to each line of output.
	  mkdir 1; cd 1
	  dotest ann-id-1 "${testcvs} -q co -l ." ''
	  module=x
	  mkdir $module
	  dotest ann-id-2 "${testcvs} add $module" \
"Directory ${CVSROOT_DIRNAME}/$module added to the repository"
	  cd $module

	  file=m
	  echo '$Id''$' > $file

	  dotest ann-id-3 "$testcvs add $file" \
"${PROG} add: scheduling file .$file. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest ann-id-4 "$testcvs -Q ci -m . $file" \
"RCS file: ${CVSROOT_DIRNAME}/$module/$file,v
done
Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
initial revision: 1\.1
done"

	  echo line2 >> $file
	  dotest ann-id-5 "$testcvs -Q ci -m . $file" \
"Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
new revision: 1\.2; previous revision: 1\.1
done"

	  # The version number after $file,v should be `1.2'.
	  # 1.9.28.1 puts `1.1' there.
	  dotest ann-id-6 "$testcvs -Q ann $file" \
"
Annotations for $file
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1.2          ($username8 *[0-9a-zA-Z-]*): "'\$'"Id: $file,v 1.1 [0-9/]* [0-9:]* $username Exp "'\$'"
1.2          ($username8 *[0-9a-zA-Z-]*): line2"

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	crerepos)
	  # Various tests relating to creating repositories, operating
	  # on repositories created with old versions of CVS, etc.

	  CVS_SERVER_save=$CVS_SERVER

	  # Because this test is all about -d options and such, it
	  # at least to some extent needs to be different for remote vs.
	  # local.
	  if $remote; then

	    # Use :ext: rather than :fork:.  Most of the tests use :fork:,
	    # so we want to make sure that we test :ext: _somewhere_.
	    # Make sure 'rsh' works first.
	    depends_on_rsh "$CVS_RSH"
	    if test $? -eq 77; then
		skip crerepos "$skipreason"
		continue
	    fi

	    # For remote, just create the repository.  We don't yet do
	    # the various other tests above for remote but that should be
	    # changed.
	    mkdir crerepos
	    mkdir crerepos/CVSROOT

            # Make sure server ignores real ${HOME}/.cvsrc:
            cat >$TESTDIR/cvs-setHome <<EOF
#!/bin/sh
HOME=$HOME
export HOME
exec $CVS_SERVER_save "\$@"
EOF
            chmod a+x $TESTDIR/cvs-setHome

	    # Note that we set CVS_SERVER at the beginning.
	    CVS_SERVER=$TESTDIR/cvs-setHome; export CVS_SERVER
	    CREREPOS_ROOT=:ext:$host$TESTDIR/crerepos
	  else

	    # First, if the repository doesn't exist at all...
	    dotest_fail crerepos-1 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} \[checkout aborted\]: ${TESTDIR}/crerepos/CVSROOT: .*"
	    mkdir crerepos

	    # The repository exists but CVSROOT doesn't.
	    dotest_fail crerepos-2 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} \[checkout aborted\]: ${TESTDIR}/crerepos/CVSROOT: .*"
	    mkdir crerepos/CVSROOT

	    # Checkout of nonexistent module
	    dotest_fail crerepos-3 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} checkout: cannot find module .cvs-sanity. - ignored"

	    # Now test that CVS works correctly without a modules file
	    # or any of that other stuff.  In particular, it *must*
	    # function if administrative files added to CVS recently (since
	    # CVS 1.3) do not exist, because the repository might have
	    # been created with an old version of CVS.
	    mkdir 1; cd 1
	    dotest crerepos-4 \
"${testcvs} -q -d ${TESTDIR}/crerepos co CVSROOT" \
''
	    if echo yes | \
${testcvs} -d ${TESTDIR}/crerepos release -d CVSROOT >>${LOGFILE}; then
	      pass crerepos-5
	    else
	      fail crerepos-5
	    fi
	    rm -rf CVS
	    cd ..
	    # The directory 1 should be empty
	    dotest crerepos-6 "rmdir 1"

	    CREREPOS_ROOT=${TESTDIR}/crerepos

	  fi

	  if $remote; then
	    # Test that CVS rejects a relative path in CVSROOT.
	    mkdir 1; cd 1
	    # Note that having the client reject the pathname (as :fork:
	    # does), does _not_ test for the bugs we are trying to catch
	    # here.  The point is that malicious clients might send all
	    # manner of things and the server better protect itself.
	    dotest_fail crerepos-6a-r \
"${testcvs} -q -d :ext:`hostname`:../crerepos get ." \
"${PROG} [a-z]*: CVSROOT may only specify a positive, non-zero, integer port (not .\.\..)\.
${PROG} [a-z]*: Perhaps you entered a relative pathname${QUESTION}
${PROG} \[[a-z]* aborted\]: Bad CVSROOT: .:ext:${hostname}:\.\./crerepos.\."
	    cd ..
	    rm -r 1
	  else # local
	    # Test that CVS rejects a relative path in CVSROOT.

	    mkdir 1; cd 1
	    # Set CVS_RSH=false since ocassionally (e.g. when CVS_RSH=ssh on
	    # some systems) some rsh implementations will block because they
	    # can look up '..' and want to ask the user about the unknown host
	    # key or somesuch.  Which error message we get depends on whether
	    # false finishes running before we try to talk to it or not.
	    dotest_fail crerepos-6a "CVS_RSH=false ${testcvs} -q -d ../crerepos get ." \
"${PROG} \[checkout aborted\]: .*" \
"${PROG} checkout: CVSROOT is set for a remote access method but your
${PROG} checkout: CVS executable doesn't support it\.
${PROG} \[checkout aborted\]: Bad CVSROOT: .\.\./crerepos.\."
	    cd ..
	    rm -r 1

	    mkdir 1; cd 1
	    dotest_fail crerepos-6b "${testcvs} -d crerepos init" \
"${PROG} init: CVSROOT must be an absolute pathname (not .crerepos.)
${PROG} init: when using local access method\.
${PROG} \[init aborted\]: Bad CVSROOT: .crerepos.\."
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch file1
	  dotest crerepos-10 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest crerepos-11 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
"${PROG} add: scheduling file .cfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
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
"${PROG} checkout: Updating first-dir
U first-dir/file1"
	  dotest crerepos-17 "${testcvs} -d ${CREREPOS_ROOT} co crerepos-dir" \
"${PROG} checkout: Updating crerepos-dir
U crerepos-dir/cfile"
	  dotest crerepos-18 "${testcvs} update" \
"${PROG} update: Updating first-dir
${PROG} update: Updating crerepos-dir"

	  cd ..

          CVS_SERVER=$CVS_SERVER_save; export CVS_SERVER

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

          rm -f $TESTDIR/cvs-setHome
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir ${TESTDIR}/crerepos
	  ;;



	crerepos-extssh)
	  # Various tests relating to creating repositories, operating
	  # on repositories created with old versions of CVS, etc.

	  CVS_SERVER_save=$CVS_SERVER

	  # Because this test is all about -d options and such, it
	  # at least to some extent needs to be different for remote vs.
	  # local.
	  if $remote; then

	    # Use :extssh: rather than :fork:.  Most of the tests use :fork:,
	    # so we want to make sure that we test :extssh: _somewhere_.
	    # Make sure 'ssh' works first.
	    depends_on_rsh "$CVS_RSH"
	    if test $? -eq 77; then
		skip crerepos "$skipreason"
		continue
	    fi

	    # For remote, just create the repository.  We don't yet do
	    # the various other tests above for remote but that should be
	    # changed.
	    mkdir crerepos
	    mkdir crerepos/CVSROOT

            # Make sure server ignores real ${HOME}/.cvsrc:
            cat >$TESTDIR/cvs-setHome <<EOF
#!/bin/sh
HOME=$HOME
export HOME
exec $CVS_SERVER_save "\$@"
EOF
            chmod a+x $TESTDIR/cvs-setHome

	    # Note that we set CVS_SERVER at the beginning.
	    CVS_SERVER=$TESTDIR/cvs-setHome; export CVS_SERVER
	    CREREPOS_ROOT=:extssh:$host$TESTDIR/crerepos
	  else

	    # First, if the repository doesn't exist at all...
	    dotest_fail crerepos-extssh-1 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} \[checkout aborted\]: ${TESTDIR}/crerepos/CVSROOT: .*"
	    mkdir crerepos

	    # The repository exists but CVSROOT doesn't.
	    dotest_fail crerepos-extssh-2 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} \[checkout aborted\]: ${TESTDIR}/crerepos/CVSROOT: .*"
	    mkdir crerepos/CVSROOT

	    # Checkout of nonexistent module
	    dotest_fail crerepos-extssh-3 \
"${testcvs} -d ${TESTDIR}/crerepos co cvs-sanity" \
"${PROG} checkout: cannot find module .cvs-sanity. - ignored"

	    # Now test that CVS works correctly without a modules file
	    # or any of that other stuff.  In particular, it *must*
	    # function if administrative files added to CVS recently (since
	    # CVS 1.3) do not exist, because the repository might have
	    # been created with an old version of CVS.
	    mkdir 1; cd 1
	    dotest crerepos-extssh-4 \
"${testcvs} -q -d ${TESTDIR}/crerepos co CVSROOT" \
''
	    if echo yes | \
${testcvs} -d ${TESTDIR}/crerepos release -d CVSROOT >>${LOGFILE}; then
	      pass crerepos-extssh-5
	    else
	      fail crerepos-extssh-5
	    fi
	    rm -rf CVS
	    cd ..
	    # The directory 1 should be empty
	    dotest crerepos-extssh-6 "rmdir 1"

	    CREREPOS_ROOT=${TESTDIR}/crerepos

	  fi

	  if $remote; then
	    # Test that CVS rejects a relative path in CVSROOT.
	    mkdir 1; cd 1
	    # Note that having the client reject the pathname (as :fork:
	    # does), does _not_ test for the bugs we are trying to catch
	    # here.  The point is that malicious clients might send all
	    # manner of things and the server better protect itself.
	    dotest_fail crerepos-extssh-6a-r \
"${testcvs} -q -d :extssh:`hostname`:../crerepos get ." \
"${PROG} [a-z]*: CVSROOT may only specify a positive, non-zero, integer port (not .\.\..)\.
${PROG} [a-z]*: Perhaps you entered a relative pathname${QUESTION}
${PROG} \[[a-z]* aborted\]: Bad CVSROOT: .:extssh:${hostname}:\.\./crerepos.\."
	    cd ..
	    rm -r 1
	  else # local
	    # Test that CVS rejects a relative path in CVSROOT.

	    mkdir 1; cd 1
	    # Set CVS_RSH=false since ocassionally (e.g. when CVS_RSH=ssh on
	    # some systems) some rsh implementations will block because they
	    # can look up '..' and want to ask the user about the unknown host
	    # key or somesuch.  Which error message we get depends on whether
	    # false finishes running before we try to talk to it or not.
	    dotest_fail crerepos-extssh-6a "CVS_RSH=false ${testcvs} -q -d ../crerepos get ." \
"${PROG} \[checkout aborted\]: .*" \
"${PROG} checkout: CVSROOT is set for a remote access method but your
${PROG} checkout: CVS executable doesn't support it\.
${PROG} \[checkout aborted\]: Bad CVSROOT: .\.\./crerepos.\."
	    cd ..
	    rm -r 1

	    mkdir 1; cd 1
	    dotest_fail crerepos-extssh-6b "${testcvs} -d crerepos init" \
"${PROG} init: CVSROOT must be an absolute pathname (not .crerepos.)
${PROG} init: when using local access method\.
${PROG} \[init aborted\]: Bad CVSROOT: .crerepos.\."
	    cd ..
	    rm -r 1
	  fi # end of tests to be skipped for remote

	  # CVS better not create a history file--if the administrator 
	  # doesn't need it and wants to save on disk space, they just
	  # delete it.
	  dotest_fail crerepos-extssh-7 \
"test -f ${TESTDIR}/crerepos/CVSROOT/history" ''

	  # Now test mixing repositories.  This kind of thing tends to
	  # happen accidentally when people work with several repositories.
	  mkdir 1; cd 1
	  dotest crerepos-extssh-8 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest crerepos-extssh-9 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch file1
	  dotest crerepos-extssh-10 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest crerepos-extssh-11 "${testcvs} -q ci -m add-it" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  mkdir 1; cd 1
	  dotest crerepos-extssh-12 "${testcvs} -d ${CREREPOS_ROOT} -q co -l ." ''
	  mkdir crerepos-dir
	  dotest crerepos-extssh-13 "${testcvs} add crerepos-dir" \
"Directory ${TESTDIR}/crerepos/crerepos-dir added to the repository"
	  cd crerepos-dir
	  touch cfile
	  dotest crerepos-extssh-14 "${testcvs} add cfile" \
"${PROG} add: scheduling file .cfile. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest crerepos-extssh-15 "${testcvs} -q ci -m add-it" \
"RCS file: ${TESTDIR}/crerepos/crerepos-dir/cfile,v
done
Checking in cfile;
${TESTDIR}/crerepos/crerepos-dir/cfile,v  <--  cfile
initial revision: 1\.1
done"
	  cd ../..
	  rm -r 1

	  mkdir 1; cd 1
	  dotest crerepos-extssh-16 "${testcvs} co first-dir" \
"${PROG} checkout: Updating first-dir
U first-dir/file1"
	  dotest crerepos-extssh-17 "${testcvs} -d ${CREREPOS_ROOT} co crerepos-dir" \
"${PROG} checkout: Updating crerepos-dir
U crerepos-dir/cfile"
	  dotest crerepos-extssh-18 "${testcvs} update" \
"${PROG} update: Updating first-dir
${PROG} update: Updating crerepos-dir"

	  cd ..

          CVS_SERVER=$CVS_SERVER_save; export CVS_SERVER

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

          rm -f $TESTDIR/cvs-setHome
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
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
	  dotest rcs-3 "${testcvs} -q log -d '1996-12-11<'" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
	  dotest rcs-4 "${testcvs} -q log -d '<3 Apr 2000 00:00'" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  # Intended behavior for "cvs annotate" is that it displays the
	  # last two digits of the year.  Make sure it does that rather
	  # than some bogosity like "100".
	  dotest rcs-4a "${testcvs} annotate file1" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1          (kingdon  24-Nov-96): This is the first line
1\.2          (kingdon  24-Nov-96): This is the third line
1\.2          (kingdon  24-Nov-96): This is the fourth line
1\.2          (kingdon  24-Nov-96): This is the fifth line
1\.2          (kingdon  24-Nov-96): This is the sixth line
1\.2          (kingdon  24-Nov-96): This is the seventh line
1\.2          (kingdon  24-Nov-96): This is the eighth line
1\.2          (kingdon  24-Nov-96): This is the ninth line
1\.2          (kingdon  24-Nov-96): This is the tenth line
1\.2          (kingdon  24-Nov-96): This is the eleventh line
1\.3          (kingdon  24-Nov-00): This is the twelfth line (and what a line it is)
1\.2          (kingdon  24-Nov-96): This is the thirteenth line"

	  # Probably should split this test into two at this point (file1
	  # above this line and file2 below), as the two share little
	  # data/setup.

	  # OK, here is another one.  This one was written by hand based on
	  # doc/RCSFILES and friends.  One subtle point is that none of
	  # the lines end with newlines; that is a feature which we
	  # should be testing.
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file2,v
head			 	1.5                 ;
     branch        1.2.6;
access ;
symbols branch:1.2.6;
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
	  # ' Match the single quote in above here doc -- for font-lock mode.

	  # First test the default branch.
	  dotest rcs-5 "${testcvs} -q update file2" "U file2"
	  dotest rcs-6 "cat file2" "branch revision"

	  # Check in a revision on the branch to force CVS to
	  # interpret every revision in the file.
	  dotest rcs-6a "$testcvs -q update -r branch file2" 'U file2'
	  echo "next branch revision" > file2
	  dotest rcs-6b "${testcvs} -q ci -m mod file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.2\.6\.2; previous revision: 1\.2\.6\.1
done"

	  # Now get rid of the default branch, it will get in the way.
	  dotest rcs-7 "${testcvs} admin -b file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
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
symbols
	branch:1.2.6;
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
next	1\.2\.6\.2;

1\.2\.6\.2
date	[0-9.]*;	author ${username};	state Exp;
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
branch revision@


1\.2\.6\.2
log
@mod
@
text
@d1 1
a1 1
next branch revision
@"

	  dotest rcs-9 "${testcvs} -q update -p -D '1970-12-31 11:30 UT' file2" \
"start revision"

	  dotest rcs-10 "${testcvs} -q update -p -D '1970-12-31 12:30 UT' file2" \
"mid revision"

	  dotest rcs-11 "${testcvs} -q update -p -D '1971-01-01 00:30 UT' file2" \
"new year revision"

	  # Same test as rcs-10, but with am/pm.
	  dotest rcs-12 "${testcvs} -q update -p -D 'December 31, 1970 12:30pm UT' file2" \
"mid revision"

	  # Same test as rcs-11, but with am/pm.
	  dotest rcs-13 "${testcvs} -q update -p -D 'January 1, 1971 12:30am UT' file2" \
"new year revision"

	  # OK, now make sure cvs log doesn't have any trouble with the
	  # newphrases and such.
	  dotest rcs-14 "${testcvs} -q log file2" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
Working file: file2
head: 1\.5
branch:
locks:
access list:
symbolic names:
	branch: 1\.2\.6
keyword substitution: kv
total revisions: 7;	selected revisions: 7
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
revision 1\.2\.6\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -1
mod
----------------------------
revision 1\.2\.6\.1
date: 1971/01/01 08:00:05;  author: joe;  state: Exp;  lines: ${PLUS}1 -1
\*\*\* empty log message \*\*\*
============================================================================="
	  # Now test each date format for "cvs log -d".
	  # Earlier than 1971-01-01
	  dotest rcs-15 "${testcvs} -q log -d '<1971-01-01 00:00 GMT' file2 \
	    | grep revision" \
"total revisions: 7;	selected revisions: 3
revision 1\.3
revision 1\.2
revision 1\.1"
	  # Later than 1971-01-01
	  dotest rcs-16 "${testcvs} -q log -d '1971-01-01 00:00 GMT<' file2 \
	    | grep revision" \
"total revisions: 7;	selected revisions: 4
revision 1\.5
revision 1\.4
revision 1\.2\.6\.2
revision 1\.2\.6\.1"
	  # Alternate syntaxes for later and earlier; multiple -d options
	  dotest rcs-17 "${testcvs} -q log -d '>1971-01-01 00:00 GMT' \
	    -d '1970-12-31 12:15 GMT>' file2 | grep revision" \
"total revisions: 7;	selected revisions: 5
revision 1\.5
revision 1\.4
revision 1\.1
revision 1\.2\.6\.2
revision 1\.2\.6\.1"
	  # Range, and single date
	  dotest rcs-18 "${testcvs} -q log -d '1970-12-31 11:30 GMT' \
	    -d '1971-01-01 00:00:05 GMT<1971-01-01 01:00:01 GMT' \
	    file2 | grep revision" \
"total revisions: 7;	selected revisions: 2
revision 1\.5
revision 1\.1"
	  # Alternate range syntax; equality
	  dotest rcs-19 "${testcvs} -q log \
	    -d '1971-01-01 01:00:01 GMT>=1971-01-01 00:00:05 GMT' \
	    file2 | grep revision" \
"total revisions: 7;	selected revisions: 2
revision 1\.5
revision 1\.4"

	  cd ..

	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	rcs2)
	  # More date tests.  Might as well do this as a separate
	  # test from "rcs", so that we don't need to perturb the
	  # "written by RCS 5.7" RCS file.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  # Significance of various dates:
	  # * At least one Y2K standard refers to recognizing 9 Sep 1999
	  #   (as an example of a pre-2000 date, I guess).
	  # * At least one Y2K standard refers to recognizing 1 Jan 2001
	  #   (as an example of a post-2000 date, I guess).
	  # * Many Y2K standards refer to 2000 being a leap year.
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file1,v
head 1.7; access; symbols; locks; strict;
1.7 date 2004.08.31.01.01.01; author sue; state; branches; next 1.6;
1.6 date 2004.02.29.01.01.01; author sue; state; branches; next 1.5;
1.5 date 2003.02.28.01.01.01; author sue; state; branches; next 1.4;
1.4 date 2001.01.01.01.01.01; author sue; state; branches; next 1.3;
1.3 date 2000.02.29.01.01.01; author sue; state; branches; next 1.2;
1.2 date 99.09.09.01.01.01; author sue; state; branches; next 1.1;
1.1 date 98.09.10.01.01.01; author sue; state; branches; next;
desc @a test file@
1.7 log @@ text @head revision@
1.6 log @@ text @d1 1
a1 1
2004 was a great year for leaping@
1.5 log @@ text @d1 1
a1 1
2003 wasn't@
1.4 log @@ text @d1 1
a1 1
two year hiatus@
1.3 log @@ text @d1 1
a1 1
2000 is also a good year for leaping@
1.2 log @@ text @d1 1
a1 1
Tonight we're going to party like it's a certain year@
1.1 log @@ text @d1 1
a1 1
Need to start somewhere@
EOF
	  # ' Match the 3rd single quote in the here doc -- for font-lock mode.

	  dotest rcs2-1 "${testcvs} -q co first-dir" 'U first-dir/file1'
	  cd first-dir

	  # 9 Sep 1999
	  dotest rcs2-2 "${testcvs} -q update -p -D '1999-09-09 11:30 UT' file1" \
"Tonight we're going to party like it's a certain year"
	  # 1 Jan 2001.
	  dotest rcs2-3 "${testcvs} -q update -p -D '2001-01-01 11:30 UT' file1" \
"two year hiatus"
	  # 29 Feb 2000
	  dotest rcs2-4 "${testcvs} -q update -p -D '2000-02-29 11:30 UT' file1" \
"2000 is also a good year for leaping"
	  # 29 Feb 2003 is invalid
	  dotest_fail rcs2-5 "${testcvs} -q update -p -D '2003-02-29 11:30 UT' file1" \
"${PROG} \[[a-z]* aborted\]: Can't parse date/time: 2003-02-29 11:30 UT"

	  dotest rcs2-6 "${testcvs} -q update -p -D 2007-01-07 file1" \
"head revision"
	  # This assumes that the clock of the machine running the tests
	  # is set to at least the year 1998 or so.  There don't seem
	  # to be a lot of ways to test the relative date code (short
	  # of something like LD_LIBRARY_PRELOAD'ing in our own
	  # getttimeofday, or hacking the CVS source with testing
	  # features, which always seems to be problematic since then
	  # someone feels like documenting them and things go downhill
	  # from there).
	  # 
	  # These tests can be expected to fail 3 times every 400 years
	  # starting Feb. 29, 2096 (because 8 years from that date would
	  # be Feb. 29, 2100, which is an invalid date -- 2100 isn't a
	  # leap year because it's divisible by 100 but not by 400).

	  dotest rcs2-7 "${testcvs} -q update -p -D '96 months' file1" \
"head revision"
	  dotest rcs2-8 "${testcvs} -q update -p -D '8 years' file1" \
"head revision"

	  cd ..
	  rm -r first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	rcs3)
	  # More RCS file tests, in particular at least some of the
	  # error handling issues.
	  mkdir ${CVSROOT_DIRNAME}/first-dir
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file1,v
head 1.1; access; symbols; locks; expand o; 1.1 date 2007.03.20.04.03.02
; author jeremiah ;state ;  branches; next;desc@@1.1log@@text@head@
EOF
	  mkdir 1; cd 1
	  # CVS requires whitespace between "desc" and its value.
	  # The rcsfile(5) manpage doesn't really seem to answer the
	  # question one way or the other (it has a grammar but almost
	  # nothing about lexical analysis).
	  dotest_fail rcs3-1 "${testcvs} -q co first-dir" \
"${PROG} \[checkout aborted\]: EOF while looking for value in RCS file ${CVSROOT_DIRNAME}/first-dir/file1,v"
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file1,v
head 1.1; access; symbols; locks; expand o; 1.1 date 2007.03.20.04.03.02
; author jeremiah ;state ;  branches; next;desc @@1.1log@@text@head@
EOF
	  # Whitespace issues, likewise.
	  dotest_fail rcs3-2 "${testcvs} -q co first-dir" \
"${PROG} \[checkout aborted\]: unexpected '.x6c' reading revision number in RCS file ${CVSROOT_DIRNAME}/first-dir/file1,v"
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file1,v
head 1.1; access; symbols; locks; expand o; 1.1 date 2007.03.20.04.03.02
; author jeremiah ;state ;  branches; next;desc @@1.1 log@@text@head@
EOF
	  # Charming array of different messages for similar
	  # whitespace issues (depending on where the whitespace is).
	  dotest_fail rcs3-3 "${testcvs} -q co first-dir" \
"${PROG} \[checkout aborted\]: EOF while looking for value in RCS file ${CVSROOT_DIRNAME}/first-dir/file1,v"
	  cat <<EOF >${CVSROOT_DIRNAME}/first-dir/file1,v
head 1.1; access; symbols; locks; expand o; 1.1 date 2007.03.20.04.03.02
; author jeremiah ;state ;  branches; next;desc @@1.1 log @@text @head@
EOF
	  dotest rcs3-4 "${testcvs} -q co first-dir" 'U first-dir/file1'

	  # Ouch, didn't expect this one.  FIXCVS.  Or maybe just remove
	  # the feature, if this is a -s problem?
	  dotest_fail rcs3-5 "${testcvs} log -s nostate first-dir/file1" \
"${DOTSTAR}ssertion.*failed${DOTSTAR}" "${DOTSTAR}failed assertion${DOTSTAR}"
	  cd first-dir
	  dotest_fail rcs3-5a "${testcvs} log -s nostate file1" \
"${DOTSTAR}ssertion.*failed${DOTSTAR}" "${DOTSTAR}failed assertion${DOTSTAR}"
	  cd ..

	  # See remote code above for rationale for cd.
	  cd first-dir
	  dotest rcs3-6 "${testcvs} log -R file1" \
"${CVSROOT_DIRNAME}/first-dir/file1,v"

	  # OK, now put an extraneous '\0' at the end.
	  ${AWK} </dev/null 'BEGIN { printf "@%c", 10 }' | ${TR} '@' '\000' \
	    >>${CVSROOT_DIRNAME}/first-dir/file1,v
	  dotest_fail rcs3-7 "${testcvs} log -s nostate file1" \
"${PROG} \[log aborted\]: unexpected '.x0' reading revision number in RCS file ${CVSROOT_DIRNAME}/first-dir/file1,v"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	rcs4)
	  # Fix a bug that shows up when checking out files by date with the
	  # "-D date" command line option.  There is code in the original to
	  # handle a special case.  If the date search finds revision 1.1 it
	  # is supposed to check whether revision 1.1.1.1 has the same date
	  # stamp, which would indicate that the file was originally brought
	  # in with "cvs import".  In that case it is supposed to return the
	  # vendor branch version 1.1.1.1.
	  # 
	  # However, there is a bug in the code. It actually compares
	  # the date of revision 1.1 for equality with the date given
	  # on the command line -- clearly wrong. This commit fixes
	  # the coding bug.
	  # 
	  # There is an additional bug which is _not_ fixed yet. 
	  # The date comparison should not be a strict
	  # equality test. It should allow a fudge factor of, say, 2-3
	  # seconds. Old versions of CVS created the two revisions
	  # with two separate invocations of the RCS "ci" command. We
	  # have many old files in the tree in which the dates of
	  # revisions 1.1 and 1.1.1.1 differ by 1 second.

          mkdir rcs4
          cd rcs4

	  mkdir imp-dir
	  cd imp-dir
	  echo 'OpenMunger sources' >file1

	  # choose a time in the past to demonstrate the problem
	  TZ=GMT touch -t 200012010123 file1

	  dotest_sort rcs4-1 \
"${testcvs} import -d -m add rcs4-dir openmunger openmunger-1_0" \
'

N rcs4-dir/file1
No conflicts created by this import'
	  echo 'OpenMunger sources release 1.1 extras' >>file1
	  TZ=GMT touch -t 200112011234 file1
	  dotest_sort rcs4-2 \
"${testcvs} import -d -m add rcs4-dir openmunger openmunger-1_1" \
'

No conflicts created by this import
U rcs4-dir/file1'
	  cd ..
	  # Next checkout the new module
	  dotest rcs4-3 \
"${testcvs} -q co rcs4-dir" \
'U rcs4-dir/file1'
	  cd rcs4-dir
	  echo 'local change' >> file1

	  # commit a local change
	  dotest rcs4-4 \
"${testcvs} -q commit -m hack file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/rcs4-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  # now see if we get version 1.1 or 1.1.1.1 when we ask for
	  # a checkout by time... it really should be 1.1.1.1 as
          # that was indeed the version that was visible at the target
	  # time.
	  dotest rcs4-5 \
"${testcvs} -q update -D 'October 1, 2001 UTC' file1" \
'[UP] file1'
	  dotest rcs4-6 \
"${testcvs} -q status file1" \
'===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1\.1\.1.*
   Repository revision:	1\.1\.1\.1	'${CVSROOT_DIRNAME}'/rcs4-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		2001\.10\.01\.00\.00\.00
   Sticky Options:	(none)'

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ../..
          rm -r rcs4
          rm -rf ${CVSROOT_DIRNAME}/rcs4-dir
	  ;;



	rcs5)
	  # Some tests of the $Log keyword and log message without a trailing
	  # EOL.  This used to look ugly and, in the worst case, could cause
	  # a seg fault due to a buffer overflow.
	  #
	  # Note that it should not be possible to create this situation via a
	  # CVS server (and any client), since the server itself inserts the
	  # trailing EOL onto log messages that are missing one.  Still, we
	  # shouldn't segfault due to a corrupt RCS file and I think that a log
	  # message without the trailing EOL doesn't actually violate the RCS
	  # spec, though it doesn't appear to be possible to create such a log
	  # message using RCS 5.7.

	  mkdir $CVSROOT_DIRNAME/rcs5
	  cat <<\EOF >$CVSROOT_DIRNAME/rcs5/file1,v
head 1.1;
access;
symbols;
locks;
expand kv;

1.1 date 2007.03.20.04.03.02; author jeremiah; state Ext;  branches; next;

desc
@@

1.1
log
@he always had very fine wine@
text
@line1
/*
EOF
echo ' * Revision history: $''Log$' >>$CVSROOT_DIRNAME/rcs5/file1,v
	  cat <<\EOF >>$CVSROOT_DIRNAME/rcs5/file1,v
 */
line5
@
EOF

          mkdir rcs5
          cd rcs5
	  dotest rcs5-1 "$testcvs -Q co rcs5"
	  dotest rcs5-2 "cat rcs5/file1" \
"line1
/\\*
 \\* Revision history: "'\$'"Log: file1,v "'\$'"
 \\* Revision history: Revision 1\.1  2007/03/20 04:03:02  jeremiah
 \\* Revision history: he always had very fine wine
 \\* Revision history:
 \\*/
line5"

	  cd ..
          rm -r rcs5
          rm -rf $CVSROOT_DIRNAME/rcs5
	  ;;



	rcs6)
	  # Test that CVS notices a specific type of corruption in the RCS
	  # archive.  In the past, this type of corruption had turned up after
	  # a user ineptly attempted to delete a revision from an arcvhive 
	  # manually.
	  mkdir rcs6; cd rcs6

	  # Make the project.
	  dotest rcs6-init-1 "$testcvs -Q co -ld top .; cd top"
	  mkdir rcs6
	  dotest rcs6-init-2 "$testcvs -Q add rcs6"
	  cd rcs6

	  # Populate it.
	  echo some words >afile
	  dotest rcs6-init-3 "$testcvs -Q add afile"
	  dotest rcs6-init-4 "$testcvs -Q ci -mnewfile afile" \
"RCS file: $CVSROOT_DIRNAME/rcs6/afile,v
done
Checking in afile;
$CVSROOT_DIRNAME/rcs6/afile,v  <--  afile
initial revision: 1\.1
done"
	  echo more words >>afile
	  dotest rcs6-init-5 "$testcvs -Q ci -mrev2 afile" \
"Checking in afile;
$CVSROOT_DIRNAME/rcs6/afile,v  <--  afile
new revision: 1\.2; previous revision: 1\.1
done"

	  # Corrupt the archive.
	  sed -e '8,12d' \
	      -e 's/^head	1\.2/head	1.1/' \
	      <$CVSROOT_DIRNAME/rcs6/afile,v \
	      >$CVSROOT_DIRNAME/rcs6/cfile,v

	  # Update used to work.
	  dotest_fail rcs6-1 "$testcvs -q up" \
"$PROG \[update aborted\]: Expected head revision 1\.1, found 1\.2\."

	  # Then a commit hosed the archive further without any warnings.
	  # Updating to an old revision (e.g. 1.1) would have reported the
	  # corruption.  A second commit would have deleted data from the
	  # file.

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  cd ../../..
	  rm -r rcs6
	  rm -rf $CVSROOT_DIRNAME/rcs6
	  ;;



	lockfiles)
	  # Tests of CVS lock files.
	  # TODO-maybe: Add a test where we arrange for a loginfo
	  # script (or some such) to ensure that locks are in place
	  # so then we can see how they are behaving.

	  mkdir 1; cd 1
	  mkdir sdir
	  mkdir sdir/ssdir
	  echo file >sdir/ssdir/file1
	  dotest lockfiles-1 \
"${testcvs} -Q import -m import-it first-dir bar baz" ""
	  cd ..

	  mkdir 2; cd 2
	  dotest lockfiles-2 "${testcvs} -q co first-dir" \
"U first-dir/sdir/ssdir/file1"
	  dotest lockfiles-3 "${testcvs} -Q co CVSROOT" ""
	  cd CVSROOT
	  echo "LockDir=${TESTDIR}/locks" >config
	  dotest lockfiles-4 "${testcvs} -q ci -m config-it" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../first-dir/sdir/ssdir
	  # The error message appears twice because Lock_Cleanup only
	  # stops recursing after the first attempt.
	  dotest_fail lockfiles-5 "${testcvs} -q update" \
"${PROG} \[update aborted\]: cannot stat ${TESTDIR}/locks: No such file or directory
${PROG} \[update aborted\]: cannot stat ${TESTDIR}/locks: No such file or directory"
	  mkdir ${TESTDIR}/locks
	  # Grumble, mumble.  Cygwin.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod u=rwx,g=r,o= ${TESTDIR}/locks"
	  else
	    chmod u=rwx,g=r,o= ${TESTDIR}/locks
	  fi
	  umask 0077
	  CVSUMASK=0077; export CVSUMASK
	  dotest lockfiles-6 "${testcvs} -q update" ""
	  # TODO: should also be testing that CVS continues to honor the
	  # umask and CVSUMASK normally.  In the case of the umask, CVS
	  # doesn't seem to use it for much (although it perhaps should).
	  dotest lockfiles-7 "ls ${TESTDIR}/locks/first-dir/sdir/ssdir" ""

	  # The policy is that when CVS creates new lock directories, they
	  # inherit the permissions from the parent directory.  CVSUMASK
	  # isn't right, because typically the reason for LockDir is to
	  # use a different set of permissions.
	  #
	  # Bah!  Cygwin!
	  if test -n "$remotehost"; then
	    dotest lockfiles-7a "$CVS_RSH $remotehost 'ls -ld ${TESTDIR}/locks/first-dir'" \
"drwxr-----.*first-dir"
	    dotest lockfiles-7b "$CVS_RSH $remotehost 'ls -ld ${TESTDIR}/locks/first-dir/sdir/ssdir'" \
"drwxr-----.*first-dir/sdir/ssdir"
	  else
	    dotest lockfiles-7a "ls -ld ${TESTDIR}/locks/first-dir" \
"drwxr-----.*first-dir"
	    dotest lockfiles-7b "ls -ld ${TESTDIR}/locks/first-dir/sdir/ssdir" \
"drwxr-----.*first-dir/sdir/ssdir"
	  fi

	  cd ../../..
	  dotest lockfiles-8 "${testcvs} -q update" ""
	  dotest lockfiles-9 "${testcvs} -q co -l ." ""

	  ###
	  ### There are race conditions in the following tests, but hopefully
	  ### the 5 seconds the first process waits to remove the lockdir and
	  ### the 30 seconds CVS waits betweens checks will be significant
	  ### enough to render the case moot.
	  ###
	  # Considers the following cases:
	  #
	  #                    Lock Present
	  # Operation          Allowed (case #)
	  #
	  #                    Read      Write
	  #                    _______   ______
	  # Read              |Yes (1)   No (3)
	  # Write             |No (7)    No (9)
	  #
	  # Tests do not appear in same ordering as table.  The odd numbering
	  # scheme maintains correspondance with a larger table on 1.12.x:
	  # 1. Read when read locks are present...
	  # 3. Don't read when write locks present...
	  # 7. Don't write when read locks are present...
	  # 9. Don't write when write locks are present...

	  # 3. Don't read when write locks present...
	  mkdir "$TESTDIR/locks/first-dir/#cvs.lock"
	  (sleep 5; rmdir "$TESTDIR/locks/first-dir/#cvs.lock")&
	  dotest lockfiles-10 "$testcvs -q co -l first-dir" \
"$PROG checkout: \[[0-9:]*\] waiting for $username's lock in $CVSROOT_DIRNAME/first-dir
$PROG checkout: \[[0-9:]*\] obtained lock in $CVSROOT_DIRNAME/first-dir"

	  # 1. Read when read locks are present...
	  touch "$TESTDIR/locks/first-dir/#cvs.rfl.test.lock"
	  dotest lockfiles-11 "$testcvs -q co -l first-dir"
	  rm "$TESTDIR/locks/first-dir/#cvs.rfl.test.lock"

	  # 7. Don't write when read locks are present...
	  echo I always have trouble coming up with witty text for the test files >>first-dir/sdir/ssdir/file1
	  touch "$TESTDIR/locks/first-dir/sdir/ssdir/#cvs.rfl.test.lock"
	  (sleep 5; rm "$TESTDIR/locks/first-dir/sdir/ssdir/#cvs.rfl.test.lock")&
	  dotest lockfiles-13 "$testcvs -q ci -mconflict first-dir" \
"$PROG commit: \[[0-9:]*\] waiting for $username's lock in $CVSROOT_DIRNAME/first-dir/sdir/ssdir
$PROG commit: \[[0-9:]*\] obtained lock in $CVSROOT_DIRNAME/first-dir/sdir/ssdir
Checking in first-dir/sdir/ssdir/file1;
$CVSROOT_DIRNAME/first-dir/sdir/ssdir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"

	  # 9. Don't write when write locks are present...
	  echo yet this poem would probably only give longfellow bile >>first-dir/sdir/ssdir/file1
	  mkdir "$TESTDIR/locks/first-dir/sdir/ssdir/#cvs.lock"
	  (sleep 5; rmdir "$TESTDIR/locks/first-dir/sdir/ssdir/#cvs.lock")&
	  dotest lockfiles-19 "$testcvs -q ci -mnot-up-to-date first-dir" \
"$PROG commit: \[[0-9:]*\] waiting for $username's lock in $CVSROOT_DIRNAME/first-dir/sdir/ssdir
$PROG commit: \[[0-9:]*\] obtained lock in $CVSROOT_DIRNAME/first-dir/sdir/ssdir
Checking in first-dir/sdir/ssdir/file1;
$CVSROOT_DIRNAME/first-dir/sdir/ssdir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"

	  # 10. Don't write when history locks are present...
	  echo have you ever heard a poem quite so vile\? >>first-dir/sdir/ssdir/file1
	  mkdir "$TESTDIR/locks/CVSROOT/#cvs.history.lock"
	  (sleep 5; rmdir "$TESTDIR/locks/CVSROOT/#cvs.history.lock")&
	  dotest lockfiles-20 "$testcvs -q ci -mnot-up-to-date first-dir" \
"Checking in first-dir/sdir/ssdir/file1;
$CVSROOT_DIRNAME/first-dir/sdir/ssdir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done
$PROG commit: \[[0-9:]*\] waiting for $username's lock in $CVSROOT_DIRNAME/CVSROOT
$PROG commit: \[[0-9:]*\] obtained lock in $CVSROOT_DIRNAME/CVSROOT"

	  dotest lockfiles-21 "$testcvs -Q tag newtag first-dir"

	  rm -f $CVSROOT_DIRNAME/CVSROOT/val-tags
	  mkdir "$TESTDIR/locks/CVSROOT/#cvs.val-tags.lock"
	  (sleep 5; rmdir "$TESTDIR/locks/CVSROOT/#cvs.val-tags.lock")&
	  dotest lockfiles-22 "$testcvs -q up -r newtag first-dir" \
"$PROG update: \[[0-9:]*\] waiting for $username's lock in $CVSROOT_DIRNAME/CVSROOT
$PROG update: \[[0-9:]*\] obtained lock in $CVSROOT_DIRNAME/CVSROOT
[UP] first-dir/sdir/ssdir/file1"

	  cd CVSROOT
	  echo "# nobody here but us comments" >config
	  dotest lockfiles-cleanup-1 "${testcvs} -q ci -m config-it" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	  cd ../..
	  # Perhaps should restore the umask and CVSUMASK to what they
	  # were before.  But the other tests "should" not care about them...
	  umask 0077
	  unset CVSUMASK
	  rm -r ${TESTDIR}/locks
	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	backuprecover)
	  # Tests to make sure we get the expected behavior
	  # when we recover a repository from an old backup
	  #
	  # Details:
	  #   Backup will be older than some developer's workspaces
	  #	This means the first attempt at an update will fail
	  #	The workaround for this is to replace the CVS
	  #	  directories with those from a "new" checkout from
	  #	  the recovered repository.  Due to this, multiple
	  #	  merges should cause conflicts (the same data
	  #	  will be merged more than once).
	  #	A workspace updated before the date of the recovered
	  #	  copy will not need any extra attention
	  #
	  # Note that backuprecover-15 is probably a failure case
	  #   If nobody else had a more recent update, the data would be lost
	  #	permanently
	  #   Granted, the developer should have been notified not to do this
	  #	by now, but still...
	  #
	  mkdir backuprecover; cd backuprecover
	  mkdir 1; cd 1
	  dotest backuprecover-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest backuprecover-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir
	  mkdir dir
	  dotest backuprecover-3 "${testcvs} add dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir added to the repository"
	  touch file1 dir/file2
	  dotest backuprecover-4 "${testcvs} -q add file1 dir/file2" \
"${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest backuprecover-5 "${testcvs} -q ci -mtest" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir/file2,v
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  echo "Line one" >>file1
	  echo "  is the place" >>file1
	  echo "    we like to begin" >>file1
	  echo "Anything else" >>file1
	  echo "  looks like" >>file1
	  echo "    a sin" >>file1
	  echo "File 2" >>dir/file2
	  echo "  is the place" >>dir/file2
	  echo "    the rest of it goes"  >>dir/file2
	  echo "Why I don't use" >>dir/file2
	  echo "  something like 'foo'" >>dir/file2
	  echo "    God only knows" >>dir/file2
	  dotest backuprecover-6 "${testcvs} -q ci -mtest" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.2; previous revision: 1\.1
done"

	  # Simulate the lazy developer
	  # (he did some work but didn't check it in...)
	  cd ../..
	  mkdir 2; cd 2
	  dotest backuprecover-7 "${testcvs} -Q co first-dir" ''
	  cd first-dir
	  sed -e "s/looks like/just looks like/" file1 >tmp; mv tmp file1
	  sed -e "s/don't use/don't just use/" dir/file2 >tmp; mv tmp dir/file2

	  # developer 1 is on a roll
	  cd ../../1/first-dir
	  echo "I need some more words" >>file1
	  echo "  to fill up this space" >>file1
	  echo "    anything else would be a disgrace" >>file1
	  echo "My rhymes cross many boundries" >>dir/file2
	  echo "  this time it's files" >>dir/file2
	  echo "    a word that fits here would be something like dials" >>dir/file2
	  dotest backuprecover-8 "${testcvs} -q ci -mtest" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.3; previous revision: 1\.2
done"

	  # Save a backup copy
	  cp -r ${CVSROOT_DIRNAME}/first-dir ${CVSROOT_DIRNAME}/backup

	  # Simulate developer 3
	  cd ../..
	  mkdir 3; cd 3
	  dotest backuprecover-9a "${testcvs} -Q co first-dir" ''
	  cd first-dir
	  echo >>file1
	  echo >>dir/file2
	  echo "Developer 1 makes very lame rhymes" >>file1
	  echo "  I think he should quit and become a mime" >>file1
	  echo "What the %*^# kind of rhyme crosses a boundry?" >>dir/file2
	  echo "  I think you should quit and get a job in the foundry" >>dir/file2
	  dotest backuprecover-9b "${testcvs} -q ci -mtest" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.4; previous revision: 1\.3
done"

	  # Developer 4 so we can simulate a conflict later...
	  cd ../..
	  mkdir 4; cd 4
	  dotest backuprecover-10 "${testcvs} -Q co first-dir" ''
	  cd first-dir
	  sed -e "s/quit and/be fired so he can/" dir/file2 >tmp; mv tmp dir/file2

	  # And back to developer 1
	  cd ../../1/first-dir
	  dotest backuprecover-11 "${testcvs} -Q update" ''
	  echo >>file1
	  echo >>dir/file2
	  echo "Oh yeah, well rhyme this" >>file1
	  echo "  developer three" >>file1
	  echo "    you want opposition" >>file1
	  echo "      you found some in me!" >>file1
	  echo "I'll give you mimes" >>dir/file2
	  echo "  and foundries galore!"  >>dir/file2
	  echo "    your head will spin" >>dir/file2
	  echo "      once you find what's in store!" >>dir/file2
	  dotest backuprecover-12 "${testcvs} -q ci -mtest" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.5; previous revision: 1\.4
done"

	  # developer 3'll do a bit of work that never gets checked in
	  cd ../../3/first-dir
	  dotest backuprecover-13 "${testcvs} -Q update" ''
	  sed -e "s/very/some extremely/" file1 >tmp; mv tmp file1
	  dotest backuprecover-14 "${testcvs} -q ci -mtest" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.6; previous revision: 1\.5
done"
	  echo >>file1
	  echo "Tee hee hee hee" >>file1
	  echo >>dir/file2
	  echo "Find what's in store?" >>dir/file2
	  echo "  Oh, I'm so sure!" >>dir/file2
	  echo "    You've got an ill, and I have the cure!"  >>dir/file2

	  # Slag the original and restore it a few revisions back
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  mv ${CVSROOT_DIRNAME}/backup ${CVSROOT_DIRNAME}/first-dir

	  # Have developer 1 try an update and lose some data
	  #
	  # Feel free to imagine the horrific scream of despair
	  cd ../../1/first-dir
	  dotest backuprecover-15 "${testcvs} update" \
"${PROG} update: Updating .
U file1
${PROG} update: Updating dir
U dir/file2"

	  # Developer 3 tries the same thing (he has an office)
	  # but fails without losing data since all of his files have
	  # uncommitted changes
	  cd ../../3/first-dir
	  dotest_fail backuprecover-16 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} \[update aborted\]: could not find desired version 1\.6 in ${CVSROOT_DIRNAME}/first-dir/file1,v"

	  # create our workspace fixin' script
	  cd ../..
	  echo \
"#!/bin/sh

# This script will copy the CVS database dirs from the checked out
# version of a newly recovered repository and replace the CVS
# database dirs in a workspace with later revisions than those in the
# recovered repository
cd repos-first-dir
DATADIRS=\`find . -name CVS -print\`
cd ../first-dir
find . -name CVS -print | xargs rm -rf
for file in \${DATADIRS}; do
	cp -r ../repos-first-dir/\${file} \${file}
done" >fixit

	  # We only need to fix the workspaces of developers 3 and 4
	  # (1 lost all her data and 2 has an update date from
	  # before the date the backup was made)
	  cd 3
	  dotest backuprecover-17 \
		"${testcvs} -Q co -d repos-first-dir first-dir" ''
	  cd ../4
	  dotest backuprecover-18 \
		"${testcvs} -Q co -d repos-first-dir first-dir" ''
	  sh ../fixit
	  cd ../3; sh ../fixit

	  # (re)commit developer 3's stuff
	  cd first-dir
	  dotest backuprecover-19 "${testcvs} -q ci -mrecover/merge" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.4; previous revision: 1\.3
done"

	  # and we should get a conflict on developer 4's stuff
	  cd ../../4/first-dir
	  dotest backuprecover-20 "${testcvs} update" \
"${PROG} update: Updating \.
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.4
Merging differences between 1\.3 and 1\.4 into file1
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in file1
C file1
${PROG} update: Updating dir
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir/file2,v
retrieving revision 1\.3
retrieving revision 1\.4
Merging differences between 1\.3 and 1\.4 into file2
rcsmerge: warning: conflicts during merge
${PROG} update: conflicts found in dir/file2
C dir/file2"
	  sed -e \
"/^<<<<<<</,/^=======/d
/^>>>>>>>/d" file1 >tmp; mv tmp file1
	  sed -e \
"/^<<<<<<</,/^=======/d
/^>>>>>>>/d
s/quit and/be fired so he can/" dir/file2 >tmp; mv tmp dir/file2
	  dotest backuprecover-21 "${testcvs} -q ci -mrecover/merge" \
"Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.5; previous revision: 1\.4
done"

	  # go back and commit developer 2's stuff to prove it can still be done
	  cd ../../2/first-dir
	  dotest backuprecover-22 "${testcvs} -Q update" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.2
retrieving revision 1\.4
Merging differences between 1\.2 and 1\.4 into file1
RCS file: ${CVSROOT_DIRNAME}/first-dir/dir/file2,v
retrieving revision 1\.2
retrieving revision 1\.5
Merging differences between 1\.2 and 1\.5 into file2"
	  dotest backuprecover-23 "${testcvs} -q ci -mtest" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done
Checking in dir/file2;
${CVSROOT_DIRNAME}/first-dir/dir/file2,v  <--  file2
new revision: 1\.6; previous revision: 1\.5
done"

	  # and restore the data to developer 1
	  cd ../../1/first-dir
	  dotest backuprecover-24 "${testcvs} -Q update" ''

	  cd ../../..
	  rm -r backuprecover
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;



        sshstdio)
          # CVS_RSH=ssh can have a problem with a non-blocking stdio
          # in some cases. So, this test is all about testing :ext:
          # with CVS_RSH=ssh. The problem is that not all machines
          # will necessarily have ssh available, so be prepared to
          # skip this test.

	  # Are we able to run find and use an ssh?
	  if $remote; then :; else
	    continue
	  fi

	  depends_on_ssh
	  if test $? -eq 77; then
            skip sshstdio "$skipreason"
	    continue
	  fi

	  SSHSTDIO_ROOT=:ext:$host$CVSROOT_DIRNAME

          mkdir sshstdio; cd sshstdio
          dotest sshstdio-1 "$testcvs -d $SSHSTDIO_ROOT -q co -l ."
          mkdir first-dir
          dotest sshstdio-2 "$testcvs add first-dir" \
  "Directory $CVSROOT_DIRNAME/first-dir added to the repository"
          cd first-dir
          a='aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
          c='aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaacaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
          # Generate 1024 lines of $a
          cnt=0
          echo $a > aaa
          while [ $cnt -lt 5 ] ; do
            cnt=`expr $cnt + 1` ;
            mv aaa aaa.old
            cat aaa.old aaa.old aaa.old aaa.old > aaa
          done
          dotest sshstdio-3 "$testcvs -q add aaa" \
"$PROG add: use .$PROG commit. to add this file permanently"
          dotest sshstdio-4 "$testcvs -q ci -mcreate aaa" \
"RCS file: $CVSROOT_DIRNAME/first-dir/aaa,v
done
Checking in aaa;
$CVSROOT_DIRNAME/first-dir/aaa,v  <--  aaa
initial revision: 1\.1
done"
          # replace lines 1, 512, 513, 1024 with $c
          sed 510q < aaa > aaa.old
          (echo $c; cat aaa.old; echo $c; \
           echo $c; cat aaa.old; echo $c) > aaa
          dotest sshstdio-5 "$testcvs -q ci -mmodify-it aaa" \
"Checking in aaa;
$CVSROOT_DIRNAME/first-dir/aaa,v  <--  aaa
new revision: 1\.2; previous revision: 1\.1
done"
          cat > wrapper.sh <<EOF
#!$TESTSHELL
exec "\$@" 2>&1 < /dev/null | cat
EOF
          chmod +x wrapper.sh
          ./wrapper.sh \
           $testcvs -z5 -Q diff --side-by-side -W 500 -r 1.1 -r 1.2 \
             aaa \
	   |sed -e \
'/^Write failed flushing stdout buffer\.$/d;
 /^write stdout: Broken pipe$/d;
 :retry;
 /Write failed flushing stdout buffer\.$/{
	N;
	s/Write failed flushing stdout buffer\.\n//;
	b retry;
}
 /write stdout: Broken pipe$/{
	N;
	s/write stdout: Broken pipe\n//;
	b retry;
}' \
          > wrapper.dif
  
          $testcvs -z5 -Q diff --side-by-side -W 500 -r 1.1 -r 1.2 \
             aaa > good.dif
  
          dotest sshstdio-6 "cmp wrapper.dif good.dif"

          if $keep; then
            echo Keeping $TESTDIR and exiting due to --keep
            exit 0
          fi
  
          cd ../..
          CVS_RSH=$save_CVS_RSH; export CVS_RSH
          rm -r sshstdio
          rm -rf $CVSROOT_DIRNAME/first-dir
          ;;



	parseroot2)
	  # Test some :ext: roots for consistancy.
	  if $remote; then :; else
	    continue
	  fi

	  depends_on_rsh "$CVS_RSH"
	  if test $? -eq 77; then
	    skip parseroot2 "$skipreason"
	    continue
	  fi

	  # Test checking out and subsequently updating with some different
	  # CVSROOTs.

	  # A standard case, hostname:dirname.
	  mkdir parseroot2; cd parseroot2
	  save_CVSROOT=$CVSROOT
	  CVSROOT=$host:$CVSROOT_DIRNAME
	  dotest parseroot2-1 "$testcvs -Q co CVSROOT"
	  cd CVSROOT
	  dotest parseroot2-2 "$testcvs -Q up"
	  cd ..

	  # A degenerate remote case, just the server name and the directory
	  # name, with no :'s to help parsing.  It can be mistaken for a
	  # relative directory name.
	  rm -r CVSROOT
	  CVSROOT=$host$CVSROOT_DIRNAME
	  dotest parseroot2-3 "$testcvs -Q co CVSROOT"
	  cd CVSROOT
	  dotest parseroot2-4 "$testcvs -Q up"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi
  
	  cd ../..
	  CVSROOT=$save_CVSROOT
	  rm -r parseroot2
	  ;;



	history)
	  # CVSROOT/history tests:
	  # history: various "cvs history" invocations
	  # basic2: Generating the CVSROOT/history file via CVS commands.

	  # Put in some data for the history file (discarding what was
	  # there before).  Note that this file format is fixed; the
	  # user may wish to analyze data from a previous version of
	  # CVS.  If we phase out this format, it should be done
	  # slowly and carefully.
	  cat >${CVSROOT_DIRNAME}/CVSROOT/history <<EOF
O3395c677|anonymous|<remote>/*0|ccvs||ccvs
O3396c677|anonymous|<remote>/src|ccvs||src
O3397c677|kingdon|<remote>/*0|ccvs||ccvs
M339cafae|nk|<remote>|ccvs/src|1.229|sanity.sh
M339cafff|anonymous|<remote>|ccvs/src|1.23|Makefile
M339dc339|kingdon|~/work/*0|ccvs/src|1.231|sanity.sh
W33a6eada|anonymous|<remote>*4|ccvs/emx||Makefile.in
C3b235f50|kingdon|<remote>|ccvs/emx|1.3|README
M3b23af50|kingdon|~/work/*0|ccvs/doc|1.281|cvs.texinfo
EOF
	  dotest history-1 "${testcvs} history -e -a" \
"O 1997-06-04 19:48 ${PLUS}0000 anonymous ccvs     =ccvs= <remote>/\*
O 1997-06-05 14:00 ${PLUS}0000 anonymous ccvs     =src=  <remote>/\*
M 1997-06-10 01:38 ${PLUS}0000 anonymous 1\.23  Makefile    ccvs/src == <remote>
W 1997-06-17 19:51 ${PLUS}0000 anonymous       Makefile\.in ccvs/emx == <remote>/emx
O 1997-06-06 08:12 ${PLUS}0000 kingdon   ccvs     =ccvs= <remote>/\*
M 1997-06-10 21:12 ${PLUS}0000 kingdon   1\.231 sanity\.sh   ccvs/src == ~/work/ccvs/src
C 2001-06-10 11:51 ${PLUS}0000 kingdon   1\.3   README      ccvs/emx == <remote>
M 2001-06-10 17:33 ${PLUS}0000 kingdon   1\.281 cvs\.texinfo ccvs/doc == ~/work/ccvs/doc
M 1997-06-10 01:36 ${PLUS}0000 nk        1\.229 sanity\.sh   ccvs/src == <remote>"

	  dotest history-2 "${testcvs} history -e -a -D '10 Jun 1997 13:00 UT'" \
"W 1997-06-17 19:51 ${PLUS}0000 anonymous       Makefile\.in ccvs/emx == <remote>/emx
M 1997-06-10 21:12 ${PLUS}0000 kingdon   1\.231 sanity\.sh   ccvs/src == ~/work/ccvs/src
C 2001-06-10 11:51 ${PLUS}0000 kingdon   1\.3   README      ccvs/emx == <remote>
M 2001-06-10 17:33 ${PLUS}0000 kingdon   1\.281 cvs\.texinfo ccvs/doc == ~/work/ccvs/doc"

	  dotest history-3 "${testcvs} history -e -a -D '10 Jun 2001 13:00 UT'" \
"M 2001-06-10 17:33 ${PLUS}0000 kingdon 1\.281 cvs\.texinfo ccvs/doc == ~/work/ccvs/doc"

	  dotest history-4 "${testcvs} history -ac sanity.sh" \
"M 1997-06-10 21:12 ${PLUS}0000 kingdon 1\.231 sanity\.sh ccvs/src == ~/work/ccvs/src
M 1997-06-10 01:36 ${PLUS}0000 nk      1\.229 sanity\.sh ccvs/src == <remote>"

	  dotest history-5 "${testcvs} history -a -xCGUWAMR README sanity.sh" \
"M 1997-06-10 21:12 ${PLUS}0000 kingdon 1\.231 sanity\.sh ccvs/src == ~/work/ccvs/src
C 2001-06-10 11:51 ${PLUS}0000 kingdon 1\.3   README    ccvs/emx == <remote>
M 1997-06-10 01:36 ${PLUS}0000 nk      1\.229 sanity\.sh ccvs/src == <remote>"

	  dotest history-6 "${testcvs} history -xCGUWAMR -a -f README -f sanity.sh" \
"M 1997-06-10 21:12 ${PLUS}0000 kingdon 1\.231 sanity\.sh ccvs/src == ~/work/ccvs/src
C 2001-06-10 11:51 ${PLUS}0000 kingdon 1\.3   README    ccvs/emx == <remote>
M 1997-06-10 01:36 ${PLUS}0000 nk      1\.229 sanity\.sh ccvs/src == <remote>"

	  dotest history-7 "${testcvs} history -xCGUWAMR -a -f sanity.sh README" \
"M 1997-06-10 21:12 ${PLUS}0000 kingdon 1\.231 sanity\.sh ccvs/src == ~/work/ccvs/src
C 2001-06-10 11:51 ${PLUS}0000 kingdon 1\.3   README    ccvs/emx == <remote>
M 1997-06-10 01:36 ${PLUS}0000 nk      1\.229 sanity\.sh ccvs/src == <remote>"

	  dotest history-8 "${testcvs} history -ca -D '1970-01-01 00:00 UT'" \
"M 1997-06-10 01:36 ${PLUS}0000 nk        1\.229 sanity.sh   ccvs/src == <remote>
M 1997-06-10 01:38 ${PLUS}0000 anonymous 1\.23  Makefile    ccvs/src == <remote>
M 1997-06-10 21:12 ${PLUS}0000 kingdon   1\.231 sanity.sh   ccvs/src == ~/work/ccvs/src
M 2001-06-10 17:33 ${PLUS}0000 kingdon   1\.281 cvs.texinfo ccvs/doc == ~/work/ccvs/doc"

	  dotest history-9 "${testcvs} history -acl" \
"M 2001-06-10 17:33 ${PLUS}0000 kingdon   1\.281 cvs.texinfo ccvs/doc == ~/work/ccvs/doc
M 1997-06-10 01:38 ${PLUS}0000 anonymous 1\.23  Makefile    ccvs/src == <remote>
M 1997-06-10 21:12 ${PLUS}0000 kingdon   1\.231 sanity.sh   ccvs/src == ~/work/ccvs/src"

	  dotest history-10 "${testcvs} history -lca -D '1970-01-01 00:00 UT'" \
"M 2001-06-10 17:33 ${PLUS}0000 kingdon   1\.281 cvs.texinfo ccvs/doc == ~/work/ccvs/doc
M 1997-06-10 01:38 ${PLUS}0000 anonymous 1\.23  Makefile    ccvs/src == <remote>
M 1997-06-10 21:12 ${PLUS}0000 kingdon   1\.231 sanity.sh   ccvs/src == ~/work/ccvs/src"

	  dotest history-11 "${testcvs} history -aw" \
"O 1997-06-04 19:48 ${PLUS}0000 anonymous ccvs =ccvs= <remote>/\*
O 1997-06-05 14:00 ${PLUS}0000 anonymous ccvs =src=  <remote>/\*
O 1997-06-06 08:12 ${PLUS}0000 kingdon   ccvs =ccvs= <remote>/\*"

	  dotest history-12 "${testcvs} history -aw -D'1970-01-01 00:00 UT'" \
"O 1997-06-04 19:48 ${PLUS}0000 anonymous ccvs =ccvs= <remote>/\*
O 1997-06-05 14:00 ${PLUS}0000 anonymous ccvs =src=  <remote>/\*
O 1997-06-06 08:12 ${PLUS}0000 kingdon   ccvs =ccvs= <remote>/\*"
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
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest big-3 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  cd ../2/first-dir
	  # The idea here is particularly to test the Rcs-diff response
	  # and the reallocing thereof, for remote.
	  dotest big-6 "${testcvs} -q update" "[UP] file1"
	  cd ../..

	  if $keep; then
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  dotest modes-3 "${testcvs} add aa" \
"${PROG} add: scheduling file .aa. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest modes-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aa,v
done
Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  # Yawn.  Cygwin.
	  if test -n "$remotehost"; then
	    dotest modes-5remotehost "$CVS_RSH $remotehost 'ls -l ${CVSROOT_DIRNAME}/first-dir/aa,v'" \
"-r--r--r-- .*"
	  else
	    dotest modes-5 "ls -l ${CVSROOT_DIRNAME}/first-dir/aa,v" \
"-r--r--r-- .*"
	  fi

	  # Test for whether we can set the execute bit.
	  chmod +x aa
	  echo change it >>aa
	  dotest modes-6 "${testcvs} -q ci -m set-execute-bit" \
"Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done"
	  # If CVS let us update the execute bit, it would be set here.
	  # But it doesn't, and as far as I know that is longstanding
	  # CVS behavior.
	  #
	  # Yeah, yeah.  Search for "Cygwin".
	  if test -n "$remotehost"; then
	    dotest modes-7remotehost "$CVS_RSH $remotehost 'ls -l ${CVSROOT_DIRNAME}/first-dir/aa,v'" \
"-r--r--r-- .*"
	  else
	    dotest modes-7 "ls -l ${CVSROOT_DIRNAME}/first-dir/aa,v" \
"-r--r--r-- .*"
	  fi

	  # OK, now manually change the modes and see what happens.
	  #
	  # Cygwin, already.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod g=r,o= ${CVSROOT_DIRNAME}/first-dir/aa,v"
	  else
	    chmod g=r,o= ${CVSROOT_DIRNAME}/first-dir/aa,v
	  fi
	  echo second line >>aa
	  dotest modes-7a "${testcvs} -q ci -m set-execute-bit" \
"Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
new revision: 1\.3; previous revision: 1\.2
done"
	  # Cygwin.
	  if test -n "$remotehost"; then
	    dotest modes-7bremotehost "$CVS_RSH $remotehost 'ls -l ${CVSROOT_DIRNAME}/first-dir/aa,v'" \
"-r--r----- .*"
	  else
	    dotest modes-7b "ls -l ${CVSROOT_DIRNAME}/first-dir/aa,v" \
"-r--r----- .*"
	  fi

	  CVSUMASK=007
	  export CVSUMASK
	  touch ab
	  # Might as well test the execute bit too.
	  chmod +x ab
	  dotest modes-8 "${testcvs} add ab" \
"${PROG} add: scheduling file .ab. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest modes-9 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/ab,v
done
Checking in ab;
${CVSROOT_DIRNAME}/first-dir/ab,v  <--  ab
initial revision: 1\.1
done"
	  if $remote; then
	    # The problem here is that the CVSUMASK environment variable
	    # needs to be set on the server (e.g. .bashrc).  This is, of
	    # course, bogus, but that is the way it is currently.
	    if test -n "$remotehost"; then
	      dotest modes-10remotehost "$CVS_RSH $remotehost 'ls -l ${CVSROOT_DIRNAME}/first-dir/ab,v'" \
"-r--r--r--.*"
	    else
	      dotest modes-10r "ls -l ${CVSROOT_DIRNAME}/first-dir/ab,v" \
"-r-xr-x---.*" "-r-xr-xr-x.*"
	    fi
	  else
	    dotest modes-10 "ls -l ${CVSROOT_DIRNAME}/first-dir/ab,v" \
"-r-xr-x---.*"
	  fi

	  # OK, now add a file on a branch.  Check that the mode gets
	  # set the same way (it is a different code path in CVS).
	  dotest modes-11 "${testcvs} -q tag -b br" 'T aa
T ab'
	  dotest modes-12 "$testcvs -q update -r br" \
'[UP] aa
U ab'
	  touch ac
	  dotest modes-13 "${testcvs} add ac" \
"${PROG} add: scheduling file .ac. for addition on branch .br.
${PROG} add: use .${PROG} commit. to add this file permanently"
	  # Not sure it really makes sense to refer to a "previous revision"
	  # when we are just now adding the file; as far as I know
	  # that is longstanding CVS behavior, for what it's worth.
	  dotest modes-14 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/ac,v
done
Checking in ac;
${CVSROOT_DIRNAME}/first-dir/Attic/ac,v  <--  ac
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  if $remote; then
	    # The problem here is that the CVSUMASK environment variable
	    # needs to be set on the server (e.g. .bashrc).  This is, of
	    # course, bogus, but that is the way it is currently.  The
	    # first match is for the :ext: method (where the CVSUMASK
	    # won't be set), while the second is for the :fork: method
	    # (where it will be).
	    if test -n "$remotehost"; then
	      dotest modes-15r \
"$CVS_RSH $remotehost 'ls -l ${CVSROOT_DIRNAME}/first-dir/Attic/ac,v'" \
"-r--r--r--.*"
	    else
	      dotest modes-15r \
"ls -l ${CVSROOT_DIRNAME}/first-dir/Attic/ac,v" \
"-r--r--r--.*" "-r--r-----.*"
	    fi
	  else
	    dotest modes-15 \
"ls -l ${CVSROOT_DIRNAME}/first-dir/Attic/ac,v" \
"-r--r-----.*"
	  fi

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  # Perhaps should restore the umask and CVSUMASK.  But the other
	  # tests "should" not care about them...
	  ;;

	modes2)
	  # More tests of file permissions in the working directory
	  # and that sort of thing.

	  # The usual setup, file first-dir/aa with two revisions.
	  mkdir 1; cd 1
	  dotest modes2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest modes2-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  dotest modes2-3 "${testcvs} add aa" \
"${PROG} add: scheduling file .aa. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest modes2-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aa,v
done
Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
initial revision: 1\.1
done"
	  echo "more money" >> aa
	  dotest modes2-5 "${testcvs} -q ci -m add" \
"Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done"

	  # OK, here is the test.  The idea is to see what
	  # No_Difference does if it can't open the file.
	  # If we don't change the st_mtime, CVS doesn't even try to read
	  # the file.  Note that some versions of "touch" require that we
	  # do this while the file is still writable.
	  touch aa
	  chmod a= aa
	  # Don't try this when permissions are broken, as with Cygwin.
	  if ls ${CVSROOT_DIRNAME}/first-dir >/dev/null 2>&1; then :; else
	    dotest_fail modes2-6 "${testcvs} -q update -r 1.1 aa" \
"${PROG} \[update aborted\]: cannot open file aa for comparing: Permission denied" \
"${PROG} \[update aborted\]: reading aa: Permission denied"
	  fi

	  chmod u+rwx aa
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	modes3)
	  # Repository permissions.  Particularly, what happens if we
	  # can't read/write in the repository.
	  # TODO: the case where we can access the repository, just not
	  # the attic (may that one can remain a fatal error, seems less
	  # useful for access control).
	  mkdir 1; cd 1
	  dotest modes3-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir second-dir
	  dotest modes3-2 "${testcvs} add first-dir second-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository
Directory ${CVSROOT_DIRNAME}/second-dir added to the repository"
	  touch first-dir/aa second-dir/ab
	  dotest modes3-3 "${testcvs} add first-dir/aa second-dir/ab" \
"${PROG} add: scheduling file .first-dir/aa. for addition
${PROG} add: scheduling file .second-dir/ab. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest modes3-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aa,v
done
Checking in first-dir/aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/second-dir/ab,v
done
Checking in second-dir/ab;
${CVSROOT_DIRNAME}/second-dir/ab,v  <--  ab
initial revision: 1\.1
done"
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod a= ${CVSROOT_DIRNAME}/first-dir"
	  else
	    chmod a= ${CVSROOT_DIRNAME}/first-dir
	  fi
	  if ls ${CVSROOT_DIRNAME}/first-dir >/dev/null 2>&1; then
	    # Avoid this test under Cygwin since permissions work differently
	    # there.
	    #
	    # This test also gets avoided under Mac OS X since the system `ls'
	    # is broken and exits with a 0 status despite the permission
	    # denied error.
	    if test -n "$remotehost"; then
	      cygwin_hack=false
	    else
	      cygwin_hack=:
	    fi
	  else
	    cygwin_hack=false
	  fi

	  cd $TESTDIR/1
	  if $cygwin_hack; then :; else
	    dotest modes3-5 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} update: Updating first-dir
${PROG} update: cannot open directory ${CVSROOT_DIRNAME}/first-dir: Permission denied
${PROG} update: skipping directory first-dir
${PROG} update: Updating second-dir"
	  fi

	  # OK, I can see why one might say the above case could be a
	  # fatal error, because normally users without access to first-dir
	  # won't have it in their working directory.  But the next
	  # one is more of a problem if it is fatal.
	  #
	  # The second text string below is for Cygwin again, and again it
	  # should really be XFAIL under Cygwin, but for now deal with the
	  # passing opendir by accepting the alternate string.
	  rm -r first-dir
	  dotest modes3-6 "${testcvs} update -dP" \
"${PROG} update: Updating .
${PROG} update: Updating CVSROOT
U ${DOTSTAR}
${PROG} update: Updating first-dir
${PROG} update: cannot open directory ${CVSROOT_DIRNAME}/first-dir: Permission denied
${PROG} update: skipping directory first-dir
${PROG} update: Updating second-dir" \
"${PROG} update: Updating .
${PROG} update: Updating CVSROOT
U ${DOTSTAR}
${PROG} update: Updating first-dir
${PROG} update: Updating second-dir"

	  cd ..
	  rm -r 1
	  chmod u+rwx ${CVSROOT_DIRNAME}/first-dir
	  rm -rf ${CVSROOT_DIRNAME}/first-dir ${CVSROOT_DIRNAME}/second-dir
	  ;;

	stamps)
	  # Test timestamps.
	  mkdir 1; cd 1
	  dotest stamps-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest stamps-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  touch aa
	  echo '$''Id$' >kw
	  # Cygwin, *cough*, puts the year in the time column until the minute
	  # is no longer the current minute.  Sleep 60 seconds to avoid this
	  # problem.
	  sleep 60
	  ls -l aa >${TESTDIR}/1/stamp.aa.touch
	  ls -l kw >${TESTDIR}/1/stamp.kw.touch
	  # "sleep 1" would suffice if we could assume ls --full-time, but
	  # that is as far as I know unique to GNU ls.  Is there some POSIX.2
	  # way to get the timestamp of a file, including the seconds?
	  sleep 60
	  dotest stamps-3 "${testcvs} add aa kw" \
"${PROG} add: scheduling file .aa. for addition
${PROG} add: scheduling file .kw. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  ls -l aa >${TESTDIR}/1/stamp.aa.add
	  ls -l kw >${TESTDIR}/1/stamp.kw.add
	  # "cvs add" should not muck with the timestamp.
	  dotest stamps-4aa \
"cmp ${TESTDIR}/1/stamp.aa.touch ${TESTDIR}/1/stamp.aa.add" ''
	  dotest stamps-4kw \
"cmp ${TESTDIR}/1/stamp.kw.touch ${TESTDIR}/1/stamp.kw.add" ''
	  sleep 60
	  dotest stamps-5 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aa,v
done
Checking in aa;
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/kw,v
done
Checking in kw;
${CVSROOT_DIRNAME}/first-dir/kw,v  <--  kw
initial revision: 1\.1
done"
	  # Cygwin, *cough*, puts the year in the time column until the minute
	  # is no longer the current minute.  Sleep 60 seconds to avoid this
	  # problem.
	  sleep 60
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
${CVSROOT_DIRNAME}/first-dir/aa,v  <--  aa
new revision: 1\.2; previous revision: 1\.1
done
Checking in kw;
${CVSROOT_DIRNAME}/first-dir/kw,v  <--  kw
new revision: 1\.2; previous revision: 1\.1
done"
	  
	  # Cygwin, *cough*, puts the year in the time column until the minute
	  # is no longer the current minute.  Sleep 60 seconds to avoid this
	  # problem.
	  sleep 60
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

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	perms)
	  # short cut around checking out and committing CVSROOT
	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/config
	  echo 'PreservePermissions=yes' > ${CVSROOT_DIRNAME}/CVSROOT/config
	  chmod 444 ${CVSROOT_DIRNAME}/CVSROOT/config

	  mkdir 1; cd 1
	  dotest perms-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest perms-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  touch foo
	  chmod 431 foo
	  dotest perms-3 "${testcvs} add foo" \
"${PROG} add: scheduling file .foo. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest perms-4 "${testcvs} -q ci -m ''" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/foo,v
done
Checking in foo;
${CVSROOT_DIRNAME}/first-dir/foo,v  <--  foo
initial revision: 1\.1
done"

	  # Test checking out files with different permissions.
	  cd ../..
	  mkdir 2; cd 2
	  dotest perms-5 "${testcvs} -q co first-dir" "U first-dir/foo"
	  cd first-dir
	  if $remote; then :; else
	    # PreservePermissions not yet implemented for remote.
	    dotest perms-6 "ls -l foo" "-r---wx--x .* foo"
	  fi

	  cd ../..
	  rm -rf 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir

	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/config
	  touch ${CVSROOT_DIRNAME}/CVSROOT/config
	  chmod 444 ${CVSROOT_DIRNAME}/CVSROOT/config
	  ;;

	symlinks)
	  # short cut around checking out and committing CVSROOT
	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/config
	  echo 'PreservePermissions=yes' > ${CVSROOT_DIRNAME}/CVSROOT/config
	  chmod 444 ${CVSROOT_DIRNAME}/CVSROOT/config

	  mkdir 1; cd 1
	  dotest symlinks-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest symlinks-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  dotest symlinks-2.1 "ln -s ${TESTDIR}/fumble slink" ""
	  dotest symlinks-3 "${testcvs} add slink" \
"${PROG} add: scheduling file .slink. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  if $remote; then
	    # Remote doesn't implement PreservePermissions, and in its
	    # absence the correct behavior is to follow the symlink.
	    dotest_fail symlinks-4r "${testcvs} -q ci -m ''" \
"${PROG} \[commit aborted\]: reading slink: No such file or directory"
	  else
	    dotest symlinks-4 "${testcvs} -q ci -m ''" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/slink,v
done
Checking in slink;
${CVSROOT_DIRNAME}/first-dir/slink,v  <--  slink
initial revision: 1\.1
done"

	    # Test checking out symbolic links.
	    cd ../..
	    mkdir 2; cd 2
	    dotest symlinks-5 "${testcvs} -q co first-dir" "U first-dir/slink"
	    cd first-dir
	    dotest symlinks-6 "ls -l slink" \
"l[rwx\-]* .* slink -> ${TESTDIR}/fumble"
	  fi

	  cd ../..
	  rm -rf 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir

	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/config
	  touch ${CVSROOT_DIRNAME}/CVSROOT/config
	  chmod 444 ${CVSROOT_DIRNAME}/CVSROOT/config
	  ;;

	symlinks2)
	  # Symlinks in working directory without PreservePermissions.
	  # Also see: symlinks: with PreservePermissions
	  # rcslib-symlink-*: symlinks in repository.
	  mkdir 1; cd 1
	  dotest symlinks2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest symlinks2-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo nonsymlink > slink
	  dotest symlinks2-3 "${testcvs} add slink" \
"${PROG} add: scheduling file .slink. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest symlinks2-4 "${testcvs} -q ci -m ''" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/slink,v
done
Checking in slink;
${CVSROOT_DIRNAME}/first-dir/slink,v  <--  slink
initial revision: 1\.1
done"
	  rm slink
	  # Choose name cvslog.* so it is in default ignore list.
	  echo second file >cvslog.file2
	  dotest symlinks2-5 "ln -s cvslog.file2 slink" ""
	  dotest symlinks2-6 "${testcvs} -q ci -m linkify" \
"Checking in slink;
${CVSROOT_DIRNAME}/first-dir/slink,v  <--  slink
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest symlinks2-7 "${testcvs} -q update -r 1.1 slink" "[UP] slink"
	  dotest symlinks2-8 "cat slink" "nonsymlink"
	  dotest symlinks2-9 "ls -l slink" "-[-rwx]* .* slink"
	  cd ../..

	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	hardlinks)
	  # short cut around checking out and committing CVSROOT
	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/config
	  echo 'PreservePermissions=yes' > ${CVSROOT_DIRNAME}/CVSROOT/config
	  chmod 444 ${CVSROOT_DIRNAME}/CVSROOT/config

	  mkdir 1; cd 1
	  dotest hardlinks-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest hardlinks-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  # Make up some ugly filenames, to test that they get
	  # encoded properly in the delta nodes.  Note that `dotest' screws
	  # up if some arguments have embedded spaces.
	  if touch aaaa
	  then
	    pass hardlinks-2.1
	  else
	    fail hardlinks-2.1
	  fi

	  if ln aaaa b.b.b.b
	  then
	    pass hardlinks-2.2
	  else
	    fail hardlinks-2.2
	  fi

	  if ln aaaa 'dd dd dd'
	  then
	    pass hardlinks-2.3
	  else
	    fail hardlinks-2.3
	  fi

	  dotest hardlinks-3 "${testcvs} add [abd]*" \
"${PROG} add: scheduling file .aaaa. for addition
${PROG} add: scheduling file .b\.b\.b\.b. for addition
${PROG} add: scheduling file .dd dd dd. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest hardlinks-4 "${testcvs} -q ci -m ''" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaaa,v
done
Checking in aaaa;
${CVSROOT_DIRNAME}/first-dir/aaaa,v  <--  aaaa
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/b\.b\.b\.b,v
done
Checking in b\.b\.b\.b;
${CVSROOT_DIRNAME}/first-dir/b\.b\.b\.b,v  <--  b\.b\.b\.b
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/dd dd dd,v
done
Checking in dd dd dd;
${CVSROOT_DIRNAME}/first-dir/dd dd dd,v  <--  dd dd dd
initial revision: 1\.1
done"
	  # Test checking out hardlinked files.
	  cd ../..
	  mkdir 2; cd 2
	  if $remote; then
	    # Remote does not implement PreservePermissions.
	    dotest hardlinks-5r "${testcvs} -q co first-dir" \
"U first-dir/aaaa
U first-dir/b\.b\.b\.b
U first-dir/dd dd dd"
	    cd first-dir
	    dotest hardlinks-6r "ls -l [abd]*" \
"-[rwx\-]* *1 .* aaaa
-[rwx\-]* *1 .* b\.b\.b\.b
-[rwx\-]* *1 .* dd dd dd"
	  else
	    dotest hardlinks-5 "${testcvs} -q co first-dir" \
"U first-dir/aaaa
U first-dir/b\.b\.b\.b
U first-dir/dd dd dd"
	    cd first-dir
	    # To make sure that the files are properly hardlinked, it
	    # would be nice to do `ls -i' and make sure all the inodes
	    # match.  But I think that would require expr to support
	    # tagged regexps, and I don't think we can rely on that.
	    # So instead we just see that each file has the right
	    # number of links. -twp
	    dotest hardlinks-6 "ls -l [abd]*" \
"-[rwx\-]* *3 .* aaaa
-[rwx\-]* *3 .* b\.b\.b\.b
-[rwx\-]* *3 .* dd dd dd"
	  fi

	  cd ../..
	  rm -rf 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir

	  rm -f ${CVSROOT_DIRNAME}/CVSROOT/config
	  touch ${CVSROOT_DIRNAME}/CVSROOT/config
	  chmod 444 ${CVSROOT_DIRNAME}/CVSROOT/config
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  touch file1
	  dotest sticky-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest sticky-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest sticky-5 "${testcvs} -q tag tag1" "T file1"
	  echo add a line >>file1
	  dotest sticky-6 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest sticky-7 "${testcvs} -q update -r tag1" "[UP] file1"
	  dotest sticky-8 "cat file1" ''
	  dotest sticky-9 "${testcvs} -q update" ''
	  dotest sticky-10 "cat file1" ''
	  touch file2
	  dotest_fail sticky-11 "${testcvs} add file2" \
"${PROG} add: cannot add file on non-branch tag tag1"
	  dotest sticky-12 "${testcvs} -q update -A" "[UP] file1
${QUESTION} file2" "${QUESTION} file2
[UP] file1"
	  dotest sticky-13 "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest sticky-14 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"

	  # Now back to tag1
	  dotest sticky-15 "${testcvs} -q update -r tag1" "[UP] file1
${PROG} update: file2 is no longer in the repository"

	  rm file1
	  dotest sticky-16 "${testcvs} rm file1" \
"${PROG} remove: scheduling .file1. for removal
${PROG} remove: use .${PROG} commit. to remove this file permanently"
	  # Hmm, this command seems to silently remove the tag from
	  # the file.  This appears to be intentional.
	  # The silently part especially strikes me as odd, though.
	  dotest sticky-17 "${testcvs} -q ci -m remove-it" ""
	  dotest sticky-18 "${testcvs} -q update -A" "U file1
U file2"
	  dotest sticky-19 "${testcvs} -q update -r tag1" \
"${PROG} update: file1 is no longer in the repository
${PROG} update: file2 is no longer in the repository"
	  dotest sticky-20 "${testcvs} -q update -A" "U file1
U file2"

	  # Now try with a numeric revision.
	  dotest sticky-21 "${testcvs} -q update -r 1.1 file1" "U file1"
	  dotest sticky-22 "${testcvs} rm -f file1" \
"${PROG} remove: cannot remove file .file1. which has a numeric sticky tag of .1\.1."
	  # The old behavior was that remove allowed this and then commit
	  # gave an error, which was somewhat hard to clear.  I mean, you
	  # could get into a long elaborate discussion of this being a
	  # conflict and two ways to resolve it, but I don't really see
	  # why CVS should have a concept of conflict that arises, not from
	  # parallel development, but from CVS's own sticky tags.

	  # Ditto with a sticky date.
	  #
	  # I'm kind of surprised that the "file1 was lost" doesn't crop
	  # up elsewhere in the testsuite.  It is a long-standing
	  # discrepency between local and remote CVS and should probably
	  # be cleaned up at some point.
	  dotest sticky-23 "${testcvs} -q update -Dnow file1" \
"${PROG} update: warning: file1 was lost
U file1" "U file1"
	  dotest sticky-24 "${testcvs} rm -f file1" \
"${PROG} remove: cannot remove file .file1. which has a sticky date of .[0-9.]*."

	  dotest sticky-25 "${testcvs} -q update -A" \
"${PROG} update: warning: file1 was lost
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
	  # "keyword2" tests "cvs update -kk -j" with text and binary files
	  # I don't think any test is testing "cvs import -k".
	  # Other keyword expansion tests:
	  #   keywordlog - $Log.
	  mkdir 1; cd 1
	  dotest keyword-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest keyword-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
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
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest keyword-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest keyword-5 "cat file1" \
'\$'"Author: ${username} "'\$'"
"'\$'"Date: [0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] "'\$'"
"'\$'"Header: ${CVSROOT_DIRNAME}/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Id: file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Locker:  "'\$'"
"'\$'"Name:  "'\$'"
"'\$'"RCSfile: file1,v "'\$'"
"'\$'"Revision: 1\.1 "'\$'"
"'\$'"Source: ${CVSROOT_DIRNAME}/first-dir/file1,v "'\$'"
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
1\.1 locked
done"

	  dotest keyword-7 "$testcvs update -kkv file1" '[UP] file1'
	  dotest keyword-8 "cat file1" \
'\$'"Author: ${username} "'\$'"
"'\$'"Date: [0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] "'\$'"
"'\$'"Header: ${CVSROOT_DIRNAME}/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Id: file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp "'\$'"
"'\$'"Locker:  "'\$'"
"'\$'"Name:  "'\$'"
"'\$'"RCSfile: file1,v "'\$'"
"'\$'"Revision: 1\.1 "'\$'"
"'\$'"Source: ${CVSROOT_DIRNAME}/first-dir/file1,v "'\$'"
"'\$'"State: Exp "'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State: Exp "'\$'" "'\$'"State: Exp "'\$'"
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  dotest keyword-9 "$testcvs update -kkvl file1" '[UP] file1'
	  dotest keyword-10 "cat file1" \
'\$'"Author: ${username} "'\$'"
"'\$'"Date: [0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] "'\$'"
"'\$'"Header: ${CVSROOT_DIRNAME}/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp ${username} "'\$'"
"'\$'"Id: file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp ${username} "'\$'"
"'\$'"Locker: ${username} "'\$'"
"'\$'"Name:  "'\$'"
"'\$'"RCSfile: file1,v "'\$'"
"'\$'"Revision: 1\.1 "'\$'"
"'\$'"Source: ${CVSROOT_DIRNAME}/first-dir/file1,v "'\$'"
"'\$'"State: Exp "'\$'"
"'\$'"Nonkey"'\$'"
"'\$'"Date
"'\$'"State: Exp "'\$'" "'\$'"State: Exp "'\$'"
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.1  [0-9/]* [0-9:]*  ${username}
xx add
xx"

	  dotest keyword-11 "${testcvs} update -kk file1" '[UP] file1'
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

	  dotest keyword-13 "$testcvs update -kv file1" '[UP] file1'
	  dotest keyword-14 "cat file1" \
"${username}
[0-9][0-9][0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]
${CVSROOT_DIRNAME}/first-dir/file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp
file1,v 1\.1 [0-9/]* [0-9:]* ${username} Exp


file1,v
1\.1
${CVSROOT_DIRNAME}/first-dir/file1,v
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest keyword-19 "${testcvs} -q tag tag1" "T file1"
	  echo "change" >> file1
	  dotest keyword-20 "${testcvs} -q ci -m mod2 file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"

	  # Prior to 1.11.23, remote CVS would fail the patch checksum test
	  # and refetch the file here, failing this test.
	  dotest keyword-21 "$testcvs -q update -r tag1" 'U file1'

	  dotest keyword-22 "cat file1" '\$'"Name: tag1 "'\$'

	  # The update used to fail the first time with a checksum failure
	  # here, then the server would send the whole failure.  This was fixed
	  # in 1.11.23.
	  dotest keyword-23 "$testcvs update -A file1" "U file1"
	  dotest keyword-24 "cat file1" '\$'"Name:  "'\$'"
change"

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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir
	  echo initial >file1
	  dotest keywordlog-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"

	  # See "rmadd" for a list of other tests of cvs ci -r.
	  dotest keywordlog-4 "${testcvs} -q ci -r 1.3 -m add file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.3
done"

	  cd ../..
	  mkdir 2; cd 2
	  dotest keywordlog-4a "${testcvs} -q co first-dir" "U first-dir/file1"
	  cd ../1/first-dir

	  echo 'xx $''Log$' >> file1
	  cat >${TESTDIR}/comment.tmp <<EOF
First log line
Second log line
EOF
	  # As with rmadd-25, "cvs ci -r" sets a sticky tag.
	  dotest_fail keywordlog-4b \
"${testcvs} ci -F ${TESTDIR}/comment.tmp file1" \
"${PROG} commit: sticky tag .1\.3. for file .file1. is not a branch
${PROG} \[commit aborted\]: correct above errors first!"
	  dotest keywordlog-4c "${testcvs} -q update -A" "M file1"

	  dotest keywordlog-5 "${testcvs} ci -F ${TESTDIR}/comment.tmp file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"
	  rm -f ${TESTDIR}/comment.tmp
	  dotest keywordlog-6 "${testcvs} -q tag -b br" "T file1"
	  dotest keywordlog-7 "cat file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"

	  cd ../../2/first-dir
	  dotest keywordlog-8 "${testcvs} -q update" "[UP] file1"
	  dotest keywordlog-9 "cat file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"
	  cd ../../1/first-dir

	  echo "change" >> file1
	  dotest keywordlog-10 "${testcvs} ci -m modify file1" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.5; previous revision: 1\.4
done"
	  dotest keywordlog-11 "cat file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.5  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"

	  cd ../../2/first-dir
	  dotest keywordlog-12 "${testcvs} -q update" "[UP] file1"
	  dotest keywordlog-13 "cat file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.5  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"

	  cd ../../1/first-dir
	  dotest keywordlog-14 "${testcvs} -q update -r br" "[UP] file1"
	  echo br-change >>file1
	  dotest keywordlog-15 "${testcvs} -q ci -m br-modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4\.2\.1; previous revision: 1\.4
done"
	  dotest keywordlog-16 "cat file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4\.2\.1  [0-9/]* [0-9:]*  ${username}
xx br-modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
br-change"
	  cd ../../2/first-dir
	  dotest keywordlog-17 "${testcvs} -q update -r br" "[UP] file1"
	  dotest keywordlog-18 "cat file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4\.2\.1  [0-9/]* [0-9:]*  ${username}
xx br-modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
br-change"
	  cd ../..
	  dotest keywordlog-19 "${testcvs} -q co -p -r br first-dir/file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4\.2\.1  [0-9/]* [0-9:]*  ${username}
xx br-modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
br-change"
	  dotest keywordlog-20 "${testcvs} -q co -p first-dir/file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.5  [0-9/]* [0-9:]*  ${username}
xx modify
xx
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx
change"
	  dotest keywordlog-21 "${testcvs} -q co -p -r 1.4 first-dir/file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"

	  cd 2/first-dir
	  # OK, the basic rule for keyword expansion is that it
	  # happens on checkout.  And the rule for annotate is that
	  # it annotates a checked-in revision, rather than a checked-out
	  # file.  So, although it is kind of confusing that the latest
	  # revision does not appear in the annotated output, and the
	  # annotated output does not quite match what you'd get with
	  # update or checkout, the behavior is more or less logical.
	  # The same issue occurs with annotate and other keywords,
	  # I think, although it is particularly noticeable for $Log.
	  dotest keywordlog-22 "${testcvs} ann -r br file1" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.3          ($username8 *[0-9a-zA-Z-]*): initial
1\.4\.2\.1      ($username8 *[0-9a-zA-Z-]*): xx "'\$'"Log: file1,v "'\$'"
1\.4\.2\.1      ($username8 *[0-9a-zA-Z-]*): xx Revision 1\.4  [0-9/]* [0-9:]*  $username
1\.4\.2\.1      ($username8 *[0-9a-zA-Z-]*): xx First log line
1\.4\.2\.1      ($username8 *[0-9a-zA-Z-]*): xx Second log line
1\.4\.2\.1      ($username8 *[0-9a-zA-Z-]*): xx
1\.4\.2\.1      ($username8 *[0-9a-zA-Z-]*): br-change"
	  dotest keywordlog-23 "${testcvs} ann -r HEAD file1" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.3          ($username8 *[0-9a-zA-Z-]*): initial
1\.5          ($username8 *[0-9a-zA-Z-]*): xx "'\$'"Log: file1,v "'\$'"
1\.5          ($username8 *[0-9a-zA-Z-]*): xx Revision 1\.4  [0-9/]* [0-9:]*  $username
1\.5          ($username8 *[0-9a-zA-Z-]*): xx First log line
1\.5          ($username8 *[0-9a-zA-Z-]*): xx Second log line
1\.5          ($username8 *[0-9a-zA-Z-]*): xx
1\.5          ($username8 *[0-9a-zA-Z-]*): change"
	  cd ../..

	  #
	  # test the operation of 'admin -o' in conjunction with keywords
	  # (especially Log - this used to munge the RCS file for all time)
	  #

	  dotest keywordlog-24 \
"${testcvs} admin -oHEAD 1/first-dir/file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
deleting revision 1\.5
done"

	  dotest keywordlog-25 \
"${testcvs} -q co -p first-dir/file1" \
"initial
xx "'\$'"Log: file1,v "'\$'"
xx Revision 1\.4  [0-9/]* [0-9:]*  ${username}
xx First log line
xx Second log line
xx"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r 1 2
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	keywordname)
	  # Test the Name keyword.
	  # See the keyword test for a descriptions of some other tests that
	  # test keyword expansion modes.
	  mkdir keywordname; cd keywordname
	  mkdir 1; cd 1
	  dotest keywordname-init-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest keywordname-init-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  echo '$'"Name$" >file1
	  echo '$'"Name$" >file2
	  dotest keywordname-init-3 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"

	  # See "rmadd" for a list of other tests of cvs ci -r.
	  dotest keywordname-init-4 "${testcvs} -q ci -r 1.3 -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.3
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.3
done"
	  dotest keywordname-init-5b "cat file1" \
'\$''Name:  \$'
	  dotest keywordname-init-5c "cat file2" \
'\$''Name:  \$'

	  dotest keywordname-init-6 "$testcvs -q up -A"
	  dotest keywordname-init-6b "cat file1" \
'\$''Name:  \$'
	  dotest keywordname-init-6c "cat file2" \
'\$''Name:  \$'

	  dotest keywordname-init-7 "${testcvs} -q tag -b br" \
"T file1
T file2"

	  echo new data >>file1
	  dotest keywordname-init-8 "${testcvs} -q ci -mchange" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.4; previous revision: 1\.3
done"

	  # First check out a branch.
	  #
	  # There used to be a bug where static tags would be substituted for
	  # Name keywords but not branch tags.
	  #
	  # Prior to 1.11.23, there also used to be a bug where keyword
	  # substitutions were not performed unless the file was otherwise
	  # updated.  When this bug was present, keywordname-update-1 would
	  # report a patch checksum failure and refetch file1 in client/server
	  # mode and no `br' would have been substituted into Name's value for
	  # file2, meaning keywordname-update-3 would also fail.
	  dotest keywordname-update-1 "$testcvs -q up -rbr" \
'U file1
U file2'
	  dotest keywordname-update-2 "cat file1" '\$''Name: br \$'

	  # For the same reason keywordname-update-1 would fail above, no `br'
	  # would have been substituted into Name's value here prior to
	  # 1.11.23.
	  dotest keywordname-update-3 "cat file2" '\$''Name: br \$'

	  # Now verify that updating to the trunk leaves no substitution for
	  # $Name
	  dotest keywordname-update-4 "${testcvs} -q tag firsttag" \
"T file1
T file2"
	  # This used to fail in the same manner as keywordname-update-1.
	  dotest keywordname-update-5 "$testcvs -q up -A" \
'U file1
U file2'
	  dotest keywordname-update-6 "cat file1" \
'\$''Name:  \$
new data'
	  dotest keywordname-update-7 "cat file2" '\$''Name:  \$'

	  # This used to fail in the same manner as keywordname-update-1.
	  dotest keywordname-update-8 "$testcvs -q up -rfirsttag" \
'U file1
U file2'
	  dotest keywordname-update-9 "cat file1" '\$''Name: firsttag \$'

	  # This used to fail in the same manner as keywordname-update-3.
	  dotest keywordname-update-10 "cat file2" '\$''Name: firsttag \$'

	  # And reverify the trunk update when the change is actually removed.
	  dotest keywordname-update-11 "$testcvs -q up -A" \
'U file1
U file2'
	  dotest keywordname-update-12 "cat file1" \
'\$''Name:  \$
new data'
	  dotest keywordname-update-13 "cat file2" '\$''Name:  \$'

	  cd ../..

	  # now verify that a fresh checkout substitutes all the $Name fields
	  mkdir 2; cd 2
	  dotest keywordname-checkout-1 \
"${testcvs} -q co -rfirsttag first-dir" \
"U first-dir/file1
U first-dir/file2"
	  cd first-dir
	  dotest keywordname-checkout-2 "cat file1" '\$'"Name: firsttag "'\$'
	  dotest keywordname-checkout-3 "cat file2" '\$'"Name: firsttag "'\$'

	  cd ../..

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  rm -r keywordname
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	keyword2)
	  # Test merging on files with keywords:
	  #   without -kk
	  #   with -kk
	  #     on text files
	  #     on binary files
	  # Note:  This test assumes that CVS has already passed the binfiles
	  #    test sequence
	  # Note2:  We are testing positive on binary corruption here
	  #    we probably really DON'T want to 'cvs update -kk' a binary file...
	  mkdir 1; cd 1
	  dotest keyword2-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest keyword2-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir

	  echo '$''Revision$' >> file1
	  echo "I" >>file1
	  echo "like" >>file1
	  echo "long" >>file1
	  echo "files!" >>file1
	  echo "" >>file1
	  echo "a test line for our times" >>file1
	  echo "" >>file1
	  echo "They" >>file1
	  echo "make" >>file1
	  echo "diff" >>file1
	  echo "look like it" >>file1
	  echo "did a much better" >>file1
	  echo "job." >>file1
	  dotest keyword2-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"

	  ${AWK} 'BEGIN { printf "%c%c%c%sRevision: 1.1 $@%c%c", \
	    2, 10, 137, "$", 13, 10 }' \
	    </dev/null | ${TR} '@' '\000' >../binfile.dat
	  cp ../binfile.dat .
	  dotest keyword2-5 "${testcvs} add -kb binfile.dat" \
"${PROG} add: scheduling file .binfile\.dat. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"

	  dotest keyword2-6 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/binfile\.dat,v
done
Checking in binfile\.dat;
${CVSROOT_DIRNAME}/first-dir/binfile\.dat,v  <--  binfile\.dat
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  dotest keyword2-7 "${testcvs} -q tag -b branch" \
"T binfile\.dat
T file1"

	  sed -e 's/our/the best of and the worst of/' file1 >f; mv f file1
	  dotest keyword2-8 "${testcvs} -q ci -m change" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"

	  dotest keyword2-9 "$testcvs -q update -r branch" \
'U binfile\.dat
[UP] file1'

	  echo "what else do we have?" >>file1
	  dotest keyword2-10 "${testcvs} -q ci -m change" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  # Okay, first a conflict in file1 - should be okay with binfile.dat
	  dotest keyword2-11 "$testcvs -q update -A -j branch" \
"U binfile\.dat
U file1
RCS file: $CVSROOT_DIRNAME/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file1
rcsmerge: warning: conflicts during merge"

	  dotest_fail keyword2-12 "${testcvs} diff file1" \
"Index: file1
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.2
diff -r1\.2 file1
0a1
> <<<<<<< file1
1a3,5
> =======
> \\\$""Revision: 1\.1\.2\.1 \\\$
> >>>>>>> 1\.1\.2\.1
14a19
> what else do we have${QUESTION}"

	  # Here's the problem... shouldn't -kk a binary file...
	  rm file1
	  dotest keyword2-13 "${testcvs} -q update -A -kk -j branch" \
"${PROG} update: warning: file1 was lost
U file1
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.1
retrieving revision 1\.1\.2\.1
Merging differences between 1\.1 and 1\.1\.2\.1 into file1"

	  # binfile won't get checked in, but it is now corrupt and could
	  # have been checked in if it had changed on the branch...
	  dotest keyword2-14 "${testcvs} -q ci -m change" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"

	  # "-kk" no longer corrupts binary files
	  dotest keyword2-15 "cmp binfile.dat ../binfile.dat" ''

	  # Okay, restore everything and make CVS try and merge a binary file...
	  # "-kk" no longer affects binary files
	  dotest keyword2-16 "${testcvs} -q update -A" \
"[UP] file1"
	  dotest keyword2-17 "${testcvs} -q tag -b branch2" \
"T binfile\.dat
T file1"
	  dotest keyword2-18 "$testcvs -q update -r branch2" \
'U binfile\.dat
[UP] file1'

	  ${AWK} 'BEGIN { printf "%c%c%c@%c%c", 2, 10, 137, 13, 10 }' \
	    </dev/null | ${TR} '@' '\000' >>binfile.dat
	  dotest keyword2-19 "${testcvs} -q ci -m badbadbad" \
"Checking in binfile\.dat;
${CVSROOT_DIRNAME}/first-dir/binfile\.dat,v  <--  binfile\.dat
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"
	  # "-kk" no longer affects binary files

	  # XXXX: do not ask, why we get the "U binfile.dat" line twice
	  #       looks like a bug!
	  dotest keyword2-20 "${testcvs} -q update -A -kk -j branch2" \
"U binfile\.dat
U binfile\.dat
U file1"

	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
	  dotest_sort head-1 "${testcvs} import -m add first-dir tag1 tag2" \
"

N first-dir/file1
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
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest head-4 "${testcvs} -q tag trunktag" "T file1
T file2"
	  echo 'add a line on trunk after trunktag' >> file1
	  dotest head-5 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3; previous revision: 1\.2
done"
	  dotest head-6 "${testcvs} -q tag -b br1" "T file1
T file2"
	  dotest head-7 "$testcvs -q update -r br1" \
'[UP] file1
[UP] file2'
	  echo 'modify on branch' >>file1
	  dotest head-8 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3\.2\.1; previous revision: 1\.3
done"
	  dotest head-9 "${testcvs} -q tag brtag" "T file1
T file2"
	  echo 'modify on branch after brtag' >>file1
	  dotest head-10 "${testcvs} -q ci -m modify" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.3\.2\.2; previous revision: 1\.3\.2\.1
done"
	  # With no sticky tags, HEAD is the head of the trunk.
	  dotest head-trunk-setup "$testcvs -q update -A" \
'[UP] file1
[UP] file2'
	  dotest head-trunk-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # and diff thinks so too.  Case (a) from the comment in
	  # cvs.texinfo (Common options).
	  dotest_fail head-trunk-diff "${testcvs} -q diff -c -r HEAD -r br1" \
"Index: file1
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.3\.2\.2
diff -c -r1\.3 -r1\.3\.2\.2
\*\*\* file1	${RFCDATE}	1\.3
--- file1	${RFCDATE}	1\.3\.2\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,3 \*\*\*\*
--- 1,5 ----
  imported contents
  add a line on trunk
  add a line on trunk after trunktag
${PLUS} modify on branch
${PLUS} modify on branch after brtag"

	  # With a branch sticky tag, HEAD is the head of the trunk.
	  dotest head-br1-setup "$testcvs -q update -r br1" \
'[UP] file1
[UP] file2'
	  dotest head-br1-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # But diff thinks that HEAD is "br1".  Case (b) from cvs.texinfo.
	  # Probably people are relying on it.
	  dotest head-br1-diff "${testcvs} -q diff -c -r HEAD -r br1" ""

	  # With a nonbranch sticky tag on a branch,
	  # HEAD is the head of the trunk
	  dotest head-brtag-setup "$testcvs -q update -r brtag" \
'[UP] file1
[UP] file2'
	  dotest head-brtag-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"

	  # CVS 1.9 and older thought that HEAD is "brtag" (this was
	  # noted as "strange, maybe accidental").  But "br1" makes a
	  # whole lot more sense.
	  dotest head-brtag-diff "${testcvs} -q diff -c -r HEAD -r br1" ""

	  # With a nonbranch sticky tag on the trunk, HEAD is the head
	  # of the trunk, I think.
	  dotest head-trunktag-setup "$testcvs -q update -r trunktag" \
'[UP] file1
[UP] file2'
	  dotest head-trunktag-check "cat file1" "imported contents
add a line on trunk"
	  dotest head-trunktag-update "${testcvs} -q update -r HEAD -p file1" \
"imported contents
add a line on trunk
add a line on trunk after trunktag"
	  # Like head-brtag-diff, there is a non-branch sticky tag.
	  dotest_fail head-trunktag-diff \
	    "${testcvs} -q diff -c -r HEAD -r br1" \
"Index: file1
===================================================================
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
retrieving revision 1\.3
retrieving revision 1\.3\.2\.2
diff -c -r1\.3 -r1\.3\.2\.2
\*\*\* file1	${RFCDATE}	1\.3
--- file1	${RFCDATE}	1\.3\.2\.2
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
\*\*\* 1,3 \*\*\*\*
--- 1,5 ----
  imported contents
  add a line on trunk
  add a line on trunk after trunktag
${PLUS} modify on branch
${PLUS} modify on branch after brtag"

	  # Also might test what happens if we setup with update -r
	  # HEAD.  In general, if sticky tags matter, does the
	  # behavior of "update -r <foo>" (without -p) depend on the
	  # sticky tags before or after the update?

	  # Note that we are testing both the case where this deletes
	  # a revision (file1) and the case where it does not (file2)
	  dotest_fail head-o0a "${testcvs} admin -o ::br1" \
"${PROG} admin: Administrating \.
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
${PROG} admin: cannot remove revision 1\.3\.2\.1 because it has tags
${PROG} admin: RCS file for .file1. not modified\.
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
	  dotest head-o0b "${testcvs} tag -d brtag" \
"${PROG} tag: Untagging \.
D file1
D file2"
	  dotest head-o1 "${testcvs} admin -o ::br1" \
"${PROG} admin: Administrating \.
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
deleting revision 1\.3\.2\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	tagdate)
	  # Test combining -r and -D.
	  #
	  # Note that this is not a complete test.  It relies on the fact
	  # that update, checkout and export have a LOT of shared code.
	  # Notice:
	  #	1)  checkout is never tested at all with -r -D
	  #	2)  update never uses an argument to '-D' besides 'now'
	  #		(this test does not provide enough data to prove
	  #		that 'cvs update' with both a '-r' and a '-D'
	  #		specified does not ignore '-D': a 'cvs up
	  #		-r<branch> -Dnow' and a 'cvs up -r<branch>'
	  #		should specify the same file revision).
	  #	3)  export uses '-r<branch> -D<when there was a different
	  #		revision>', hopefully completing this behavior test
	  #		for checkout and update as well.
	  #
	  mkdir 1; cd 1
	  save_TZ=$TZ
	  TZ=UTC; export TZ
	  dotest tagdate-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest tagdate-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  echo trunk-1 >file1
	  dotest tagdate-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest tagdate-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  date_T1=`getrlogdate -r1.1 first-dir/file1`

	  dotest tagdate-5 "${testcvs} -q tag -b br1" "T file1"
	  dotest tagdate-6 "${testcvs} -q tag -b br2" "T file1"
	  echo trunk-2 >file1
	  dotest tagdate-7 "${testcvs} -q ci -m modify-on-trunk" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done"
	  date_T2=`getrlogdate -r1.2 first-dir/file1`

	  # We are testing -r -D where br1 is a (magic) branch without
	  # any revisions.  First the case where br2 doesn't have any
	  # revisions either:
	  dotest tagdate-8 "${testcvs} -q update -p -r br1 -D now" "trunk-1"
	  dotest tagdate-9 "${testcvs} -q update -r br2" "[UP] file1"
	  echo br2-1 >file1
	  dotest tagdate-10 "${testcvs} -q ci -m modify-on-br2" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"
	  date_T3=`getrlogdate -r1.1.4.1 first-dir/file1`

	  # Then the case where br2 does have revisions:
	  dotest tagdate-11 "${testcvs} -q update -p -r br1 -D now" "trunk-1"

	  # For some reason, doing this on a branch seems to be relevant.
	  dotest_fail tagdate-12 "${testcvs} -q update -j:yesterday" \
"${PROG} \[update aborted\]: argument to join may not contain a date specifier without a tag"
	  # And check export

	  echo br2-2 >file1
	  dotest tagdate-13 "${testcvs} -q ci -m modify-2-on-br2" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.2; previous revision: 1\.1\.4\.1
done"
	  date_T4=`getrlogdate -r1.1.4.2 first-dir/file1`

	  cd ../..
	  mkdir 2; cd 2
	  dotest tagdate-14 "${testcvs} -q export -r br2 -D'$date_T3' first-dir" \
"[UP] first-dir/file1"
	  dotest tagdate-15 "cat first-dir/file1" "br2-1"

	  # Now for annotate
	  cd ../1/first-dir
	  dotest tagdate-16 "${testcvs} annotate -rbr2 -D'$date_T3'" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1\.4\.1      ($username8 *[0-9a-zA-Z-]*): br2-1"

	  dotest tagdate-17 "${testcvs} annotate -rbr2 -Dnow" \
"
Annotations for file1
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
1\.1\.4\.2      ($username8 *[0-9a-zA-Z-]*): br2-2"

	  # Now check to see what happens when we add files to br2 and trunk
	  echo br2-1 > file3
	  dotest tagdate-18 "${testcvs} add file3" \
"${PROG} add: scheduling file \`file3' for addition on branch \`br2'
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest tagdate-19 "${testcvs} -q ci -m add file3" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  date_T5=`getrlogdate -r1.1 first-dir/file3`
	  date_T6=`getrlogdate -r1.1.2.1 first-dir/file3`

	  cd ../..
	  mkdir 3; cd 3
	  dotest tagdate-20 "${testcvs} -Q co first-dir" ''
	  cd first-dir
	  echo trunk-1 > file2
	  dotest tagdate-21 "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest tagdate-22 "${testcvs} -q ci -m add file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  date_T7=`getrlogdate -r1.1 first-dir/file2`
	  echo "trunk-2" >file2
	  dotest tagdate-23 "${testcvs} -q ci -m update file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.2; previous revision: 1\.1
done"
	  date_T8=`getrlogdate -r1.2 first-dir/file2`

	  cd ../../1/first-dir
	  echo br2-1 > file2
	  dotest tagdate-24 "${testcvs} add file2" \
"${PROG} add: scheduling file \`file2' for addition on branch \`br2'
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest tagdate-25 "${testcvs} -q ci -m add file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.2\.2\.2; previous revision: 1\.2\.2\.1
done"
	  date_T9=`getrlogdate -r1.2.2.2 first-dir/file2`
	  cd ../..

	  # Time  Rev     Branch  Comments
	  # T0            trunk   first-dir created
	  # T1    1.1     trunk   first-dir/file1 committed "trunk-1"
	  #               br1     branch created
	  #               br2     branch created
	  # T2    1.2     trunk   first-dir/file1 committed "trunk-2"
	  # T3    1.1.4.1 br2     first-dir/file1 committed "br2-1"
	  # +60s
	  # T4    1.1.4.2 br2     first-dir/file1 committed "br2-2"
	  # T5    1.1     trunk   first-dir/file3 dead
	  # T6    1.1.2.1 br2     first-dir/file3 committed "br2-1"
	  # T7    1.1     trunk   first-dir/file2 committed "trunk-1"
	  # T8    1.2     trunk   first-dir/file2 committed "trunk-2"
	  # T8    1.2.2.1 br2     first-dir/file2 dead
	  # T9    1.2.2.2 br2     first-dir/file2 committed "br2-1"
	  # 

	  mkdir 4; cd 4
	  (echo Dates for tagdate-26-* are:;\
	   echo "  date_T1='$date_T1'";\
	   echo "  date_T2='$date_T2'";\
	   echo "  date_T3='$date_T3'";\
	   echo "  date_T4='$date_T4'";\
	   echo "  date_T5='$date_T5'";\
	   echo "  date_T6='$date_T6'";\
	   echo "  date_T7='$date_T7'";\
	   echo "  date_T8='$date_T8'";\
	   echo "  date_T9='$date_T9'") >>$LOGFILE
	  dotest tagdate-26-trunk-t1 \
"${testcvs} co -D'$date_T1' -d first-dir-trunk-t1 first-dir" \
"${PROG} checkout: Updating first-dir-trunk-t1
U first-dir-trunk-t1/file1"
	  dotest tagdate-26-br2-t1 \
"${testcvs} co -r br2 -D'$date_T1' -d first-dir-br2-t1 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t1
U first-dir-br2-t1/file1"
	  dotest tagdate-26-trunk-t2 \
"${testcvs} co -D'$date_T2' -d first-dir-trunk-t2 first-dir" \
"${PROG} checkout: Updating first-dir-trunk-t2
U first-dir-trunk-t2/file1"
	  dotest tagdate-26-br2-t2 \
"${testcvs} co -r br2 -D'$date_T2' -d first-dir-br2-t2 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t2
U first-dir-br2-t2/file1"
	  dotest tagdate-26-br2-t3 \
"${testcvs} co -r br2 -D'$date_T3' -d first-dir-br2-t3 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t3
U first-dir-br2-t3/file1"
	  dotest tagdate-26-br2-t4 \
"${testcvs} co -r br2 -D'$date_T4' -d first-dir-br2-t4 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t4
U first-dir-br2-t4/file1"
	  dotest tagdate-26-br2-t6 \
"${testcvs} co -r br2 -D'$date_T6' -d first-dir-br2-t6 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t6
U first-dir-br2-t6/file1
U first-dir-br2-t6/file3"
	  dotest tagdate-26-trunk-t7 \
"${testcvs} co -D'$date_T7' -d first-dir-trunk-t7 first-dir" \
"${PROG} checkout: Updating first-dir-trunk-t7
U first-dir-trunk-t7/file1
U first-dir-trunk-t7/file2"
	  dotest tagdate-26-br2-t7 \
"${testcvs} co -r br2 -D'$date_T7' -d first-dir-br2-t7 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t7
U first-dir-br2-t7/file1
U first-dir-br2-t7/file3"
	  dotest tagdate-26-trunk-t8 \
"${testcvs} co -D'$date_T8' -d first-dir-trunk-t8 first-dir" \
"${PROG} checkout: Updating first-dir-trunk-t8
U first-dir-trunk-t8/file1
U first-dir-trunk-t8/file2"
	  dotest tagdate-26-br2-t8 \
"${testcvs} co -r br2 -D'$date_T8' -d first-dir-br2-t8 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t8
U first-dir-br2-t8/file1
U first-dir-br2-t8/file3"
	  dotest tagdate-26-br2-t9 \
"${testcvs} co -r br2 -D'$date_T9' -d first-dir-br2-t9 first-dir" \
"${PROG} checkout: Updating first-dir-br2-t9
U first-dir-br2-t9/file1
U first-dir-br2-t9/file2
U first-dir-br2-t9/file3"
	  dotest tagdate-27-trunk-t1 \
"${testcvs} status first-dir-trunk-t1" \
"${PROG} status: Examining first-dir-trunk-t1
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1[^.]*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		[0-9.]*
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t1 \
"${testcvs} status first-dir-br2-t1" \
"${PROG} status: Examining first-dir-br2-t1
===================================================================
File: file1            	Status: Needs Patch

   Working revision:	1\.1[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-trunk-t2 \
"${testcvs} status first-dir-trunk-t2" \
"${PROG} status: Examining first-dir-trunk-t2
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.2[^.]*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		[0-9.]*
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t2 \
"${testcvs} status first-dir-br2-t2" \
"${PROG} status: Examining first-dir-br2-t2
===================================================================
File: file1            	Status: Needs Patch

   Working revision:	1\.1[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t3 \
"${testcvs} status first-dir-br2-t3" \
"${PROG} status: Examining first-dir-br2-t3
===================================================================
File: file1            	Status: Needs Patch

   Working revision:	1\.1\.4\.1[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t4 \
"${testcvs} status first-dir-br2-t4" \
"${PROG} status: Examining first-dir-br2-t4
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1\.4\.2[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t6 \
"${testcvs} status first-dir-br2-t6" \
"${PROG} status: Examining first-dir-br2-t6
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1\.4\.2[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1\.2\.1[^.]*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
   Sticky Tag:		br2 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-trunk-t7 \
"${testcvs} status first-dir-trunk-t7" \
"${PROG} status: Examining first-dir-trunk-t7
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.2[^.]*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		[0-9.]*
   Sticky Options:	(none)

===================================================================
File: file2            	Status: Up-to-date

   Working revision:	1\.1[^.]*
   Repository revision:	1\.1	${CVSROOT_DIRNAME}/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		[0-9.]*
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t7 \
"${testcvs} status first-dir-br2-t7" \
"${PROG} status: Examining first-dir-br2-t7
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1\.4\.2[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1\.2\.1[^.]*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
   Sticky Tag:		br2 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-trunk-t8 \
"${testcvs} status first-dir-trunk-t8" \
"${PROG} status: Examining first-dir-trunk-t8
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.2[^.]*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		(none)
   Sticky Date:		[0-9.]*
   Sticky Options:	(none)

===================================================================
File: file2            	Status: Up-to-date

   Working revision:	1\.2[^.]*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		[0-9.]*
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t8 \
"${testcvs} status first-dir-br2-t8" \
"${PROG} status: Examining first-dir-br2-t8
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1\.4\.2[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1\.2\.1[^.]*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
   Sticky Tag:		br2 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  dotest tagdate-27-br2-t9 \
"${testcvs} status first-dir-br2-t9" \
"${PROG} status: Examining first-dir-br2-t9
===================================================================
File: file1            	Status: Up-to-date

   Working revision:	1\.1\.4\.2[^.]*
   Repository revision:	1\.1\.4\.2	${CVSROOT_DIRNAME}/first-dir/file1,v
   Sticky Tag:		br2 (branch: 1\.1\.4)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file2            	Status: Up-to-date

   Working revision:	1\.2\.2\.2[^.]*
   Repository revision:	1\.2\.2\.2	${CVSROOT_DIRNAME}/first-dir/file2,v
   Sticky Tag:		br2 (branch: 1\.2\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file3            	Status: Up-to-date

   Working revision:	1\.1\.2\.1[^.]*
   Repository revision:	1\.1\.2\.1	${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
   Sticky Tag:		br2 (branch: 1\.1\.2)
   Sticky Date:		(none)
   Sticky Options:	(none)"

	  # Now check the contents of the files
	  dotest tagdate-28-trunk-t1 'cat first-dir-trunk-t1/file1' 'trunk-1'
	  dotest tagdate-28-br2-t1 'cat first-dir-br2-t1/file1' 'trunk-1'
	  dotest tagdate-28-trunk-t2 'cat first-dir-trunk-t2/file1' 'trunk-2'
	  dotest tagdate-28-br2-t2 'cat first-dir-br2-t2/file1' 'trunk-1'
	  dotest tagdate-28-br2-t3 'cat first-dir-br2-t3/file1' 'br2-1'
	  dotest tagdate-28-br2-t4 'cat first-dir-br2-t4/file1' 'br2-2'
	  dotest tagdate-28-br2-t6a 'cat first-dir-br2-t6/file1' "br2-2"
	  dotest tagdate-28-br2-t6b 'cat first-dir-br2-t6/file3' "br2-1"
	  dotest tagdate-28-trunk-t7a 'cat first-dir-trunk-t7/file1' "trunk-2"
	  dotest tagdate-28-trunk-t7b 'cat first-dir-trunk-t7/file2' "trunk-1"
	  dotest tagdate-28-br2-t7a 'cat first-dir-br2-t7/file1' "br2-2"
	  dotest tagdate-28-br2-t7b 'cat first-dir-br2-t7/file3' "br2-1"
	  dotest tagdate-28-trunk-t8a 'cat first-dir-trunk-t8/file1' "trunk-2"
	  dotest tagdate-28-trunk-t8b 'cat first-dir-trunk-t8/file2' "trunk-2"
	  dotest tagdate-28-br2-t8a 'cat first-dir-br2-t8/file1' "br2-2"
	  dotest tagdate-28-br2-t8c 'cat first-dir-br2-t8/file3' "br2-1"
	  dotest tagdate-28-br2-t9a 'cat first-dir-br2-t9/file1' "br2-2"
	  dotest tagdate-28-br2-t9b 'cat first-dir-br2-t9/file2' "br2-1"
	  dotest tagdate-28-br2-t9c 'cat first-dir-br2-t9/file3' "br2-1"
	  cd ..

	  unset date_T1 date_T2 date_T3 date_T4 date_T5
	  unset date_T6 date_T7 date_T8 date_T9
	  TZ=$save_TZ

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  rm -r 1 2 3 4
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
	  cd first-dir

	  echo trunk-1 >file1
	  echo trunk-1 >file2
	  dotest multibranch2-3 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest multibranch2-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  dotest multibranch2-5 "${testcvs} -q tag -b A" "T file1
T file2"
	  dotest multibranch2-6 "${testcvs} -q tag -b B" "T file1
T file2"

	  dotest multibranch2-7 "$testcvs -q update -r B" \
'[UP] file1
[UP] file2'
	  echo branch-B >file1
	  echo branch-B >file2
	  dotest multibranch2-8 "${testcvs} -q ci -m modify-on-B" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.4\.1; previous revision: 1\.1
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.4\.1; previous revision: 1\.1
done"

	  dotest multibranch2-9 "${testcvs} -q update -r A" '[UP] file1
[UP] file2'
	  echo branch-A >file1
	  # When using cvs-1.9.20, this commit gets a failed assertion in rcs.c.
	  dotest multibranch2-10 "${testcvs} -q ci -m modify-on-A" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"

	  dotest multibranch2-11 "${testcvs} -q log file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
	  dotest multibranch2-12 "${testcvs} -q log -r1.1 file1" \
"
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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

	  # OK, try very much the same thing except we run update -j to
	  # bring the changes from B to A.  Probably tests many of the
	  # same code paths but might as well keep it separate, I guess.

	  dotest multibranch2-13 "${testcvs} -q update -r B" "[UP] file1
[UP] file2"
	  dotest multibranch2-14 "${testcvs} -q update -r A -j B file2" \
"[UP] file2
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
retrieving revision 1.1
retrieving revision 1.1.4.1
Merging differences between 1.1 and 1.1.4.1 into file2"
	  dotest multibranch2-15 "${testcvs} -q ci -m commit-on-A file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
	  ;;

	tag8k)
	  # In cvs-1.9.27, there is a bug that can cause an abort.
	  # It happens when you commit a change to a ,v file that has
	  # just the right amount of tag/branch info to align one of the
	  # semicolons in the branch info to be on a 8k-byte boundary.
	  # The result: rcsbuf_getkey got an abort.  This failure doesn't
	  # corrupt the ,v file -- that would be really serious.  But it
	  # does leave stale write locks that have to be removed manually.

	  mkdir 1
	  cd 1

	  module=x

	  : > junk
	  dotest tag8k-1 "$testcvs -Q import -m . $module X Y" ''
	  dotest tag8k-2 "$testcvs -Q co $module" ''
	  cd $module

	  file=m
	  : > $file
	  dotest tag8k-3 "$testcvs add $file" \
"${PROG} add: scheduling file .$file. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest tag8k-4 "$testcvs -Q ci -m . $file" \
"RCS file: ${CVSROOT_DIRNAME}/$module/$file,v
done
Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
initial revision: 1\.1
done"

	  # It seems there have to be at least two versions.
	  echo a > $file
	  dotest tag8k-5 "$testcvs -Q ci -m . $file" \
"Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
new revision: 1\.2; previous revision: 1\.1
done"

	  # Add just under 8K worth of tags.
	  t=TAG---------------------------------------------------------------------
	  t=$t$t
	  t=$t$t$t$t$t
	  # Now $t is 720 bytes long.

	  # Apply some tags with that long prefix.
	  dotest tag8k-6  "$testcvs -Q tag $t-0 $file" ''
	  dotest tag8k-7  "$testcvs -Q tag $t-1 $file" ''
	  dotest tag8k-8  "$testcvs -Q tag $t-2 $file" ''
	  dotest tag8k-9  "$testcvs -Q tag $t-3 $file" ''
	  dotest tag8k-10 "$testcvs -Q tag $t-4 $file" ''
	  dotest tag8k-11 "$testcvs -Q tag $t-5 $file" ''
	  dotest tag8k-12 "$testcvs -Q tag $t-6 $file" ''
	  dotest tag8k-13 "$testcvs -Q tag $t-7 $file" ''
	  dotest tag8k-14 "$testcvs -Q tag $t-8 $file" ''
	  dotest tag8k-15 "$testcvs -Q tag $t-9 $file" ''
	  dotest tag8k-16 "$testcvs -Q tag $t-a $file" ''

	  # Extract the author value.
	  name=`sed -n 's/.*;	author \([^;]*\);.*/\1/p' ${CVSROOT_DIRNAME}/$module/$file,v|sed 1q`

	  # Form a suffix string of length (16 - length($name)).
	  # CAREFUL: this will lose if $name is longer than 16.
	  sed_pattern=`echo $name|sed s/././g`
	  suffix=`echo 1234567890123456|sed s/$sed_pattern//`

	  # Add a final tag with length chosen so that it will push the
	  # offset of the `;' in the 2nd occurrence of `;\tauthor' in the
	  # ,v file to exactly 8192.
	  dotest tag8k-17 "$testcvs -Q tag "x8bytes-$suffix" $file" ''

	  # This commit would fail with 1.9.27.
	  echo a >> $file
	  dotest tag8k-18 "$testcvs -Q ci -m . $file" \
"Checking in $file;
${CVSROOT_DIRNAME}/$module/$file,v  <--  $file
new revision: 1\.3; previous revision: 1\.2
done"
	  cd ../..
	  rm -r 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
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
	  #   basicb-o* (attempt to delete all revisions)
	  #   basica-o1 through basica-o3 (basic :: usage)
	  #   head-o1 (::branch, where this deletes a revision or is noop)
	  #   branches-o1 (::branch, similar, with different branch topology)
	  #   log-o1 (1.3.2.1::)
	  #   binfiles-o1 (1.3:: and ::1.3; binary files)
	  #   binfiles3-9 (binary files)
	  #   Also could be testing:
	  #     1.3.2.6::1.3.2.8
	  #     1.3.2.6::1.3.2
	  #     1.3.2.1::1.3.2.6
	  #     1.3::1.3.2.6 (error?  or synonym for ::1.3.2.6?)
	  # -n: admin, tagf tests.

	  mkdir 1; cd 1
	  dotest admin-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest admin-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir

	  dotest_fail admin-3 "${testcvs} -q admin -i file1" \
"${PROG} admin: the -i option to admin is not supported
${PROG} admin: run add or import to create an RCS file
${PROG} \[admin aborted\]: specify ${PROG} -H admin for usage information"
	  dotest_fail admin-4 "${testcvs} -q log file1" \
"${PROG} log: nothing known about file1"
	  dotest_fail admin-4a "${testcvs} -q admin file1" \
"${PROG} admin: nothing known about file1"

	  # Set up some files, file2 a plain one and file1 with a revision
	  # on a branch.
	  touch file1 file2
	  dotest admin-5 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest admin-6 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  dotest admin-7 "${testcvs} -q tag -b br" "T file1
T file2"
	  dotest admin-8 "$testcvs -q update -r br" \
'U file1
U file2'
	  echo 'add a line on the branch' >> file1
	  echo 'add a file on the branch' >> file3
	  dotest admin-9a "${testcvs} -q add file3" \
"${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest admin-9b "${testcvs} -q ci -m modify-on-branch" \
"Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
new revision: 1\.1\.2\.1; previous revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done
Checking in file3;
${CVSROOT_DIRNAME}/first-dir/Attic/file3,v  <--  file3
new revision: 1\.1\.2\.1; previous revision: 1\.1
done"
	  dotest admin-10 "$testcvs -q update -A" \
"U file1
U file2
$PROG update: file3 is no longer in the repository"

	  # Check that we can administer files in the repository that
	  # aren't in the working directory.
	  dotest admin-10-1 "${testcvs} admin ." \
"${PROG} admin: Administrating .
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
	  dotest admin-10-2 "${testcvs} -q admin file3" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done"

	  # Try to recurse with a numeric revision arg.
	  # If we wanted to comprehensive about this, we would also test
	  # this for -l, -u, and all the different -o syntaxes.
	  dotest_fail admin-10a "${testcvs} -q admin -b1.1.2" \
"${PROG} [a-z]*: while processing more than one file:
${PROG} \[[a-z]* aborted\]: attempt to specify a numeric revision"
	  dotest_fail admin-10b "${testcvs} -q admin -m1.1:bogus file1 file2" \
"${PROG} [a-z]*: while processing more than one file:
${PROG} \[[a-z]* aborted\]: attempt to specify a numeric revision"

	  # try a bad symbolic revision
	  dotest_fail admin-10c "${testcvs} -q admin -bBOGUS" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file1,v: Symbolic name BOGUS is undefined.
${PROG} admin: RCS file for .file1. not modified\.
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file2,v: Symbolic name BOGUS is undefined.
${PROG} admin: RCS file for .file2. not modified\."

	  # Note that -s option applies to the new default branch, not
	  # the old one.
	  # Also note that the implementation of -a via "rcs" requires
	  # no space between -a and the argument.  However, we expect
	  # to change that once CVS parses options.
	  dotest admin-11 "${testcvs} -q admin -afoo,bar -abaz \
-b1.1.2 -cxx -U -sfoo file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest admin-11a "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
	  dotest admin-12 "${testcvs} -q admin -bbr file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest admin-12a "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
	  # Note that we must accept either 2 or 4 digit year.
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
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches
	1\.1\.2\.1;
next	;

1\.1\.2\.1
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state foo;
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
	  dotest_fail admin-14-1 "${testcvs} -q admin \
-m1.1.1.1:changed-bogus-log-message file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
cvs admin: ${CVSROOT_DIRNAME}/first-dir/file2,v: no such revision 1\.1\.1\.1
cvs admin: RCS file for .file2. not modified."
	  dotest admin-14-2 "${testcvs} -q log file2" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
Working file: file2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	br: 1\.1\.0\.2
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
add
============================================================================="

	  dotest admin-14-3 "${testcvs} -q admin -aauth3 -aauth2,foo \
-soneone:1.1 -m1.1:changed-log-message -ntagone: file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
	  dotest admin-15 "${testcvs} -q log file2" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest admin-17 "${testcvs} -q log file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file1,v: symbolic name br already bound to 1\.1
${PROG} admin: RCS file for .file1. not modified\."
	  dotest admin-19 "${testcvs} -q admin -ebaz -ebar,auth3 -nbr file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest admin-20 "${testcvs} -q log file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
${PROG} admin: Couldn't open rcs file .${TESTDIR}/foo/bar.: No such file or directory
${PROG} \[admin aborted\]: cannot continue"

	  # In the remote case, we are cd'd off into the temp directory
	  # and so these tests give "No such file or directory" errors.
	  if $remote; then :; else
	    dotest admin-19a-admin "${testcvs} -q admin -A../../${CVSROOTDIR}/first-dir/file2,v file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	    dotest admin-19a-log "${testcvs} -q log -h -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
	  fi # end of tests skipped for remote

	  # Now test that plain -e works right.
	  dotest admin-19a-2 "${testcvs} -q admin -e file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"
	  dotest admin-19a-3 "${testcvs} -q log -h -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
keyword substitution: kv
total revisions: 2
============================================================================="

	  # Put the access list back, to avoid special cases later.
	  dotest admin-19a-4 "${testcvs} -q admin -afoo,auth2 file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"

	  # Add another revision to file2, so we can delete one.
	  echo 'add a line' >> file2
	  dotest admin-21 "${testcvs} -q ci -m modify file2" \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest admin-22 "${testcvs} -q admin -o1.1 file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
deleting revision 1\.1
done"
	  # Test admin -o.  More variants that we could be testing:
	  # * REV: [on branch]
	  # * REV1:REV2 [deleting whole branch]
	  # * high branch numbers (e.g. 1.2.2.3.2.3)
	  # ... and probably others.  See RCS_delete_revs for ideas.

	  echo first rev > aaa
	  dotest admin-22-o1 "${testcvs} add aaa" \
"${PROG} add: scheduling file .aaa. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest admin-22-o2 "${testcvs} -q ci -m first aaa" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
done
Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
initial revision: 1\.1
done"
	  echo second rev >> aaa
	  dotest admin-22-o3 "${testcvs} -q ci -m second aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.2; previous revision: 1\.1
done"
	  echo third rev >> aaa
	  dotest admin-22-o4 "${testcvs} -q ci -m third aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.3; previous revision: 1\.2
done"
	  echo fourth rev >> aaa
	  dotest admin-22-o5 "${testcvs} -q ci -m fourth aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.4; previous revision: 1\.3
done"
	  echo fifth rev >>aaa
	  dotest admin-22-o6 "${testcvs} -q ci -m fifth aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.5; previous revision: 1\.4
done"
	  echo sixth rev >> aaa
	  dotest admin-22-o7 "${testcvs} -q ci -m sixth aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.6; previous revision: 1\.5
done"
	  dotest admin-22-o8 "${testcvs} admin -l1.6 aaa" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
1\.6 locked
done"
	  dotest admin-22-o9 "${testcvs} log -r1.6 aaa" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/aaa,v: can't remove locked revision 1\.6
${PROG} admin: RCS file for .aaa. not modified\."
	  dotest admin-22-o11 "${testcvs} admin -u aaa" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
1\.6 unlocked
done"
	  dotest admin-22-o12 "${testcvs} admin -o1.5: aaa" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
deleting revision 1\.6
deleting revision 1\.5
done"
	  dotest admin-22-o13 "${testcvs} log aaa" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
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
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.1; previous revision: 1\.3
done"
	  dotest_fail admin-22-o17 "${testcvs} admin -o1.2:1.4 aaa" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
deleting revision 1\.4
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/aaa,v: can't remove branch point 1\.3
${PROG} admin: RCS file for .aaa. not modified\."
	  dotest admin-22-o18 "${testcvs} update -p -r1.4 aaa" \
"===================================================================
Checking out aaa
RCS:  ${CVSROOT_DIRNAME}/first-dir/aaa,v
VERS: 1\.4
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*
first rev
second rev
third rev
fourth rev"
	  echo second branch rev >> aaa
	  dotest admin-22-o19 "${testcvs} ci -m branch-two aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.2; previous revision: 1\.3\.2\.1
done"
	  echo third branch rev >> aaa
	  dotest admin-22-o20 "${testcvs} ci -m branch-three aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.3; previous revision: 1\.3\.2\.2
done"
	  echo fourth branch rev >> aaa
	  dotest admin-22-o21 "${testcvs} ci -m branch-four aaa" \
"Checking in aaa;
${CVSROOT_DIRNAME}/first-dir/aaa,v  <--  aaa
new revision: 1\.3\.2\.4; previous revision: 1\.3\.2\.3
done"
	  dotest admin-22-o22 "${testcvs} admin -o:1.3.2.3 aaa" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
deleting revision 1\.3\.2\.1
deleting revision 1\.3\.2\.2
deleting revision 1\.3\.2\.3
done"
	  dotest admin-22-o23 "${testcvs} log aaa" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
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
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
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
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches
	1\.1\.2\.1;
next	;

1\.1\.2\.1
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state foo;
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

	  # Tests of cvs admin -n.  Make use of the results of
	  # admin-1 through admin-25.
	  # FIXME: We probably shouldn't make use of those results;
	  # this test is way too long as it is.

	  # tagtwo should be a revision
	  #
	  dotest admin-26-1 "${testcvs} admin -ntagtwo:tagone file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
      	  
	  # br1 should be a branch
	  #
	  dotest admin-26-2 "${testcvs} admin -nbr1:br file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
      	  
	  # Attach some tags using RCS versions
	  #
	  dotest admin-26-3 "${testcvs} admin -ntagthree:1.1 file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"

	  dotest admin-26-4 "${testcvs} admin -nbr2:1.1.2 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"

	  dotest admin-26-5 "${testcvs} admin -nbr4:1.1.0.2 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
      	  
	  # Check results so far
	  #
	  dotest admin-26-6 "${testcvs} status -v file2" \
"===================================================================
File: file2            	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT_DIRNAME}/first-dir/file2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	br4                      	(branch: 1\.1\.2)
	br2                      	(branch: 1\.1\.2)
	tagthree                 	(revision: 1\.1)
	br1                      	(branch: 1\.1\.2)
	tagtwo                   	(revision: 1\.1)
	tagone                   	(revision: 1\.1)
	br                       	(branch: 1\.1\.2)"

      	  
	  # Add a couple more revisions
	  #
	  echo "nuthr_line" >> file2
	  dotest admin-27-1 "${testcvs} commit -m nuthr_line file2"  \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.3; previous revision: 1\.2
done"

	  echo "yet_another" >> file2
	  dotest admin-27-2 "${testcvs} commit -m yet_another file2"  \
"Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
new revision: 1\.4; previous revision: 1\.3
done"
      	  
	  # Fail trying to reattach existing tag with -n
	  #
	  dotest admin-27-3 "${testcvs} admin -ntagfour:1.1 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"

	  dotest_fail admin-27-4 "${testcvs} admin -ntagfour:1.3 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file2,v: symbolic name tagfour already bound to 1\.1
${PROG} admin: RCS file for .file2. not modified\."
      	  
	  # Succeed at reattaching existing tag, using -N
	  #
	  dotest admin-27-5 "${testcvs} admin -Ntagfour:1.3 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done"
      	  
	  # Fail on some bogus operations
	  # Try to attach to nonexistant tag
	  #
	  dotest_fail admin-28-1 "${testcvs} admin -ntagsix:tagfive file2" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file2,v: Symbolic name or revision tagfive is undefined\.
${PROG} admin: RCS file for .file2. not modified\."
      	  
	  # Try a some nonexisting numeric target tags
	  #
	  dotest_fail admin-28-2 "${testcvs} admin -ntagseven:2.1 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} \[admin aborted\]: revision .2\.1. does not exist"

	  dotest_fail admin-28-3 "${testcvs} admin -ntageight:2.1.2 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} \[admin aborted\]: revision .2\.1\.2. does not exist"
      	  
	  # Try some invalid targets
	  #
	  dotest_fail admin-28-4 "${testcvs} admin -ntagnine:1.a.2 file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} \[admin aborted\]: tag .1\.a\.2. must start with a letter"

	  # Confirm that a missing tag is not a fatal error.
	  dotest admin-28-5.1 "${testcvs} -Q tag BO+GUS file1" ''
	  dotest_fail admin-28-5.2 "${testcvs} admin -ntagten:BO+GUS file2 file1"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file2,v: Symbolic name or revision BO${PLUS}GUS is undefined\.
${PROG} admin: RCS file for .file2. not modified\.
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done"

	  dotest_fail admin-28-6 "${testcvs} admin -nq.werty:tagfour file2"  \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} \[admin aborted\]: tag .q\.werty. must not contain the characters ..*"

	  # Verify the archive
	  #
	  dotest admin-29 "cat ${CVSROOT_DIRNAME}/first-dir/file2,v" \
"head	1\.4;
access
	auth3
	auth2
	foo;
symbols
	tagfour:1\.3
	br4:1\.1\.0\.2
	br2:1\.1\.0\.2
	tagthree:1\.1
	br1:1\.1\.0\.2
	tagtwo:1\.1
	tagone:1\.1
	br:1\.1\.0\.2;
locks; strict;
comment	@# @;


1\.4
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches;
next	1\.3;

1\.3
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches;
next	1\.2;

1\.2
date	[0-9][0-9]*\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]\.[0-9][0-9];	author ${username};	state Exp;
branches;
next	;


desc
@@


1\.4
log
@yet_another
@
text
@add a line
nuthr_line
yet_another
@


1\.3
log
@nuthr_line
@
text
@d3 1
@


1\.2
log
@modify
@
text
@d2 1
@"

	  dotest_fail admin-30 "${testcvs} admin -mbr:another-log-message \
file2 aaa file3" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/file2,v: no such revision br: 1\.1
${PROG} admin: RCS file for .file2. not modified.
RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/aaa,v: no such revision br
${PROG} admin: RCS file for .aaa. not modified.
RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
done"
	  dotest admin-31 "${testcvs} log" \
"${PROG} log: Logging \.

RCS file: ${CVSROOT_DIRNAME}/first-dir/aaa,v
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
=============================================================================

RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
Working file: file1
head: 1\.1
branch:
locks: strict
access list:
	foo
	auth2
symbolic names:
	tagten: 1\.1
	BO${PLUS}GUS: 1\.1
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
=============================================================================

RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
Working file: file2
head: 1\.4
branch:
locks: strict
access list:
	auth3
	auth2
	foo
symbolic names:
	tagfour: 1\.3
	br4: 1\.1\.0\.2
	br2: 1\.1\.0\.2
	tagthree: 1\.1
	br1: 1\.1\.0\.2
	tagtwo: 1\.1
	tagone: 1\.1
	br: 1\.1\.0\.2
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.4
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
yet_another
----------------------------
revision 1\.3
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
nuthr_line
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
modify
=============================================================================

RCS file: ${CVSROOT_DIRNAME}/first-dir/Attic/file3,v
Working file: file3
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
	br: 1\.1\.0\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: dead;
branches:  1\.1\.2;
file file3 was initially added on branch br\.
----------------------------
revision 1\.1\.2\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
another-log-message
============================================================================="

	  cd ../..
	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi
	  # clean up our after ourselves
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
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir
	  touch file1
	  dotest reserved-3 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest reserved-4 "${testcvs} -q ci -m add" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done"

	  dotest reserved-5 "${testcvs} -q admin -l file1" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
1\.1 locked
done"
	  dotest reserved-6 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
1\.1 unlocked
done"

	  dotest reserved-8 "${testcvs} log -N file1" "
RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
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
line=\`grep <\$1/\$2,v 'locks $anyusername:1\.[0-9];'\`
if test -z "\$line"; then
  # It isn't locked
  exit 0
else
  user=\`echo \$line | sed -e 's/locks \\($anyusername\\):[0-9.]*;.*/\\1/'\`
  version=\`echo \$line | sed -e 's/locks $anyusername:\\([0-9.]*\\);.*/\\1/'\`
  echo "\$user has file a-lock locked for version  \$version" >&2
  exit 1
fi
EOF
	  # Cygwin.  Blaaarg.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod +x ${TESTDIR}/lockme"
	  else
	    chmod +x ${TESTDIR}/lockme
	  fi

	  echo stuff > a-lock
	  dotest reserved-9 "${testcvs} add a-lock" \
"${PROG} add: scheduling file .a-lock. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest reserved-10 "${testcvs} -q ci -m new a-lock" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a-lock,v
done
Checking in a-lock;
${CVSROOT_DIRNAME}/first-dir/a-lock,v  <--  a-lock
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
${CVSROOT_DIRNAME}/CVSROOT/commitinfo,v  <--  commitinfo
new revision: 1\.2; previous revision: 1\.1
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..; cd first-dir

	  # Simulate (approximately) what a-lock would look like
	  # if someone else had locked revision 1.1.
	  sed -e 's/locks; strict;/locks fred:1.1; strict;/' ${CVSROOT_DIRNAME}/first-dir/a-lock,v > a-lock,v
	  # Cygwin.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod 644 ${CVSROOT_DIRNAME}/first-dir/a-lock,v"
	  else
	    chmod 644 ${CVSROOT_DIRNAME}/first-dir/a-lock,v
	  fi
	  dotest reserved-13 "mv a-lock,v ${CVSROOT_DIRNAME}/first-dir/a-lock,v"
	  # Cygwin.  Blah.
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost "chmod 444 ${CVSROOT_DIRNAME}/first-dir/a-lock,v"
	  else
	    chmod 444 ${CVSROOT_DIRNAME}/first-dir/a-lock,v
	  fi
	  echo more stuff >> a-lock
	  dotest_fail reserved-13b "${testcvs} ci -m '' a-lock" \
"fred has file a-lock locked for version  1\.1
${PROG} commit: Pre-commit check failed
${PROG} \[commit aborted\]: correct above errors first!"
	  # OK, now test "cvs admin -l" in the case where someone
	  # else has the file locked.
	  dotest_fail reserved-13c "${testcvs} admin -l a-lock" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a-lock,v
${PROG} \[admin aborted\]: Revision 1\.1 is already locked by fred"

	  dotest reserved-14 "${testcvs} admin -u1.1 a-lock" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/a-lock,v
${PROG} admin: ${CVSROOT_DIRNAME}/first-dir/a-lock,v: revision 1\.1 locked by fred; breaking lock
1\.1 unlocked
done"
	  dotest reserved-15 "${testcvs} -q ci -m success a-lock" \
"Checking in a-lock;
${CVSROOT_DIRNAME}/first-dir/a-lock,v  <--  a-lock
new revision: 1\.2; previous revision: 1\.1
done"

	  # Now test for a bug involving branches and locks
	  sed -e 's/locks; strict;/locks fred:1.2; strict;/' ${CVSROOT_DIRNAME}/first-dir/a-lock,v > a-lock,v
	  chmod 644 ${CVSROOT_DIRNAME}/first-dir/a-lock,v
	  dotest reserved-16 \
"mv a-lock,v ${CVSROOT_DIRNAME}/first-dir/a-lock,v" ""
	  chmod 444 ${CVSROOT_DIRNAME}/first-dir/a-lock,v
	  dotest reserved-17 "${testcvs} -q tag -b br a-lock" "T a-lock"
	  dotest reserved-18 "$testcvs -q update -r br a-lock" '[UP] a-lock'
	  echo edit it >>a-lock
	  dotest reserved-19 "${testcvs} -q ci -m modify a-lock" \
"Checking in a-lock;
${CVSROOT_DIRNAME}/first-dir/a-lock,v  <--  a-lock
new revision: 1\.2\.2\.1; previous revision: 1\.2
done"

	  # undo commitinfo changes
	  cd ../CVSROOT
	  echo '# vanilla commitinfo' >commitinfo
	  dotest reserved-cleanup-1 "${testcvs} -q ci -m back commitinfo" \
"Checking in commitinfo;
${CVSROOT_DIRNAME}/CVSROOT/commitinfo,v  <--  commitinfo
new revision: 1\.3; previous revision: 1\.2
done
${PROG} commit: Rebuilding administrative file database"
	  cd ..; rm -r CVSROOT; cd first-dir

	  cd ../..
	  rm -r 1
	  rm ${TESTDIR}/lockme
	  rm -rf ${CVSROOT_DIRNAME}/first-dir
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
	  # and countersuits do we discover that this was all CVS's
	  # fault.
	  #
	  # Luckily, this problem has been fixed now, as our test will
	  # handily confirm, no doubt:

	  # First make a repository containing the original text:

	  # We should be here anyway, but cd to it just in case:
	  cd ${TESTDIR}

	  mkdir diffmerge1
	  cd diffmerge1

	  # These are the files we both start out with:
	  mkdir import
	  cd import
	  diffmerge_create_older_files

	  dotest diffmerge1_import \
	    "${testcvs} import -m import diffmerge1 tag1 tag2" \
	    "${DOTSTAR}No conflicts created by this import"
	  cd ..

	  # Check out two working copies, one for "you" and one for
	  # "me".  If no branch is used and cvs detects that only one
	  # of the two people made changes, then cvs does not run the
	  # merge algorithm.  But if a branch is used, then cvs does run
	  # the merge algorithm (even in this case of only one of the two
	  # people having made changes).  CVS used to have a bug in this
	  # case.  Therefore, it is important to test this case by
	  # using a branch:
	  ${testcvs} rtag     -b tag diffmerge1 >/dev/null 2>&1
	  ${testcvs} checkout -r tag diffmerge1 >/dev/null 2>&1
	  mv diffmerge1 yours
	  ${testcvs} checkout diffmerge1 >/dev/null 2>&1
	  mv diffmerge1 mine

	  # In your working copy, you'll make changes, and
	  # then check in your changes before I check in mine:
	  cd yours
	  diffmerge_create_your_files
          dotest diffmerge1_yours "${testcvs} -q ci -m yours" \
"Checking in testcase01;
${CVSROOT_DIRNAME}/diffmerge1/testcase01,v  <--  testcase01
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase02;
${CVSROOT_DIRNAME}/diffmerge1/testcase02,v  <--  testcase02
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase03;
${CVSROOT_DIRNAME}/diffmerge1/testcase03,v  <--  testcase03
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase04;
${CVSROOT_DIRNAME}/diffmerge1/testcase04,v  <--  testcase04
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase05;
${CVSROOT_DIRNAME}/diffmerge1/testcase05,v  <--  testcase05
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase06;
${CVSROOT_DIRNAME}/diffmerge1/testcase06,v  <--  testcase06
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase07;
${CVSROOT_DIRNAME}/diffmerge1/testcase07,v  <--  testcase07
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase08;
${CVSROOT_DIRNAME}/diffmerge1/testcase08,v  <--  testcase08
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase09;
${CVSROOT_DIRNAME}/diffmerge1/testcase09,v  <--  testcase09
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done
Checking in testcase10;
${CVSROOT_DIRNAME}/diffmerge1/testcase10,v  <--  testcase10
new revision: 1\.1\.1\.1\.2\.1; previous revision: 1\.1\.1\.1
done"

	  # Change my copy.  Then I
	  # update, after both my modifications and your checkin:
	  cd ../mine
	  diffmerge_create_my_files
	  dotest diffmerge1_mine "${testcvs} -q update -j tag" \
"M testcase01
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase01,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase01
M testcase02
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase02,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase02
M testcase03
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase03,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase03
M testcase04
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase04,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase04
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase05,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase05
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase06,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase06
M testcase07
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase07,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase07
testcase07 already contains the differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1
M testcase08
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase08,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase08
M testcase09
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase09,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase09
M testcase10
RCS file: ${CVSROOT_DIRNAME}/diffmerge1/testcase10,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.1\.1\.1\.2\.1
Merging differences between 1\.1\.1\.1 and 1\.1\.1\.1\.2\.1 into testcase10"

	  # So if your changes didn't make it into my working copy, or
	  # in any case if the files do not look like the final text
	  # in the files in directory comp_me, then the test flunks:
	  cd ..
	  mkdir comp_me
	  cd comp_me
	  diffmerge_create_expected_files
	  cd ..
	  rm mine/.#*

	  # If you have GNU's version of diff, you may try
	  # uncommenting the following line which will give more
	  # fine-grained information about how cvs differed from the
	  # correct result:
	  #dotest diffmerge1_cmp "diff -u --recursive --exclude=CVS comp_me mine" ''
	  dotest diffmerge1_cmp "directory_cmp comp_me mine"

	  # Clean up after ourselves:
	  cd ..
	  if $keep; then :; else
	    rm -rf diffmerge1 ${CVSROOT_DIRNAME}/diffmerge1
	  fi
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

	  cd ..
	  rm -rf diffmerge2
	  rm -rf ${CVSROOT_DIRNAME}/diffmerge2
	  ;;

	release)
	  # Tests of "cvs release", particularly multiple arguments.
	  # Other CVS release tests:
	  #   info-cleanup-0 for "cvs -n release".
	  #   ignore-193 for the text of the question that cvs release asks.
	  #     Also for interactions with cvsignore.
	  #   basicc: "-d .", global -Q, no arguments (is a noop),
	  #     "cvs release" without -d, multiple arguments.
	  #   dirs-4: repository directory has been deleted.
	  #   modules2-6: multiple arguments.

	  # First the usual setup; create a directory first-dir.
	  mkdir 1; cd 1
	  dotest release-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest release-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir
	  mkdir dir1
	  dotest release-3 "${testcvs} add dir1" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir1 added to the repository"
	  mkdir dir2
	  dotest release-4 "${testcvs} add dir2" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir2 added to the repository"
          cd dir2
	  mkdir dir3
	  dotest release-5 "${testcvs} add dir3" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir2/dir3 added to the repository"

          cd ../..
	  dotest release-6 "${testcvs} release -d first-dir/dir2/dir3 first-dir/dir1" \
"You have .0. altered files in this repository.
Are you sure you want to release (and delete) directory .first-dir/dir2/dir3.: \
You have .0. altered files in this repository.
Are you sure you want to release (and delete) directory .first-dir/dir1.: " <<EOF
yes
yes
EOF
	  dotest_fail release-7 "test -d first-dir/dir1" ''
	  dotest_fail release-8 "test -d first-dir/dir2/dir3" ''
	  dotest release-9 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} update: Updating first-dir
${PROG} update: Updating first-dir/dir2"

          cd first-dir
	  mkdir dir1
	  dotest release-10 "${testcvs} add dir1" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir1 added to the repository"
          cd dir2
	  mkdir dir3
	  dotest release-11 "${testcvs} add dir3" \
"Directory ${CVSROOT_DIRNAME}/first-dir/dir2/dir3 added to the repository"

          cd ../..
	  dotest release-12 "${testcvs} release first-dir/dir2/dir3 first-dir/dir1" \
"You have .0. altered files in this repository.
Are you sure you want to release directory .first-dir/dir2/dir3.: .. .release. aborted by user choice.
You have .0. altered files in this repository.
Are you sure you want to release directory .first-dir/dir1.: " <<EOF
no
yes
EOF
	  dotest release-13 "${testcvs} release first-dir/dir2/dir3 first-dir/dir2" \
"You have .0. altered files in this repository.
Are you sure you want to release directory .first-dir/dir2/dir3.: \
You have .0. altered files in this repository.
Are you sure you want to release directory .first-dir/dir2.: " <<EOF
yes
yes
EOF
	  dotest release-14 "test -d first-dir/dir1" ''
	  dotest release-15 "test -d first-dir/dir2/dir3" ''
	  rm -rf first-dir/dir1 first-dir/dir2

	  dotest release-16 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} update: Updating first-dir"

	  # Check to make sure release isn't overwriting a
	  # CVS/Entries file in the current directory (using data
	  # from the released directory).

	  # cvs 1.11 (remote) fails on release-21 (a message about
          # chdir into the removed directory), although it seemingly
	  # unedits and removes the directory correctly.  If
	  # you manually continue, it then fails on release-22 do
	  # to the messed up CVS/Entries file from release-21.
          cd first-dir
	  mkdir second-dir
	  dotest release-18 "$testcvs add second-dir" \
"Directory $CVSROOT_DIRNAME/first-dir/second-dir added to the repository"

	  cd second-dir
	  touch file1
	  dotest release-19 "$testcvs -Q add file1"
	  dotest release-20 '$testcvs -q ci -m add' \
"RCS file: $CVSROOT_DIRNAME/first-dir/second-dir/file1,v
done
Checking in file1;
$CVSROOT_DIRNAME/first-dir/second-dir/file1,v  <--  file1
initial revision: 1\.1
done"
	  dotest release-21 "$testcvs edit file1"
	  cd ..
	  dotest release-22 "echo yes | $testcvs release -d second-dir" \
"You have \[0\] altered files in this repository.
Are you sure you want to release (and delete) directory \`second-dir': "
	  dotest release-23 "$testcvs -q update -d" "U second-dir/file1"
	  dotest release-24 "$testcvs edit"

	  cd ../..
	  rm -rf 1 $CVSROOT_DIRNAME/first-dir
	  ;;



	recase)
	  #
	  # Some tests of behavior which broke at one time or another when run
	  # from case insensitive clients against case sensitive servers.
	  #
	  # These tests are namned according to the following convention:
	  #
	  #   ci	Client (sandbox filesystem) case Insensitive
	  #   cs	Client (sandbox filesystem) case Sensitive
	  #   si	Server (repository filesystem) case Insensitive
	  #   ss	Server (repository filesystem) case Sensitive
	  #

	  mkdir 1; cd 1

	  # First, we will expect different results for a few of these tests
	  # based on whether the repository is on a case sensitive filesystem
	  # or not and whether the sandbox is on a case sensitive filesystem or
	  # not, so determine which cases we are dealing with:
	  echo file >file
	  echo FiLe >FiLe
	  if cmp file FiLe >/dev/null; then
	    client_sensitive=false
	  else
	    client_sensitive=:
	  fi
	  if test -n "$remotehost"; then
	    $CVS_RSH $remotehost 'echo file >file'
	    $CVS_RSH $remotehost 'echo FiLe >FiLe'
	    if $CVS_RSH $remotehost 'cmp file FiLe >/dev/null'; then
	      server_sensitive=false
	    else
	      server_sensitive=:
	    fi
	  else
	    server_sensitive=$client_sensitive
	  fi

	  # The first test (recase-1 & recase-2) is for a remove of a file then
	  # a readd in a different case.
	  mkdir $CVSROOT_DIRNAME/first-dir
	  dotest recase-init-1 "$testcvs -Q co first-dir"	
	  cd first-dir

	  echo this file has no content >file
	  dotest recase-init-2 "$testcvs -Q add file"
	  dotest recase-init-3 "$testcvs -Q ci -madd" \
"RCS file: $CVSROOT_DIRNAME/first-dir/file,v
done
Checking in file;
$CVSROOT_DIRNAME/first-dir/file,v  <--  file
initial revision: 1\.1
done"
	  dotest recase-init-4 "$testcvs -Q tag first"

	  # Now remove the file.
	  dotest recase-init-5 "$testcvs -Q rm -f file"
	  dotest recase-init-6 "$testcvs -Q ci -mrm" \
"Removing file;
$CVSROOT_DIRNAME/first-dir/file,v  <--  file
new revision: delete; previous revision: 1\.1
done"

	  # Now the test - readd in a different case.
	  echo this file needs some content >FiLe
	  if $server_sensitive; then
	    dotest recase-1ss "$testcvs add FiLe" \
"$PROG add: scheduling file \`FiLe' for addition
$PROG add: use '$PROG commit' to add this file permanently"
	    dotest recase-2ss "$testcvs -q ci -mrecase" \
"RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
done
Checking in FiLe;
$CVSROOT_DIRNAME/first-dir/FiLe,v  <--  FiLe
initial revision: 1\.1
done"
	  else # server insensitive
	    dotest recase-1si "$testcvs add FiLe" \
"$PROG add: Re-adding file \`FiLe' (in place of dead revision 1\.2)\.
$PROG add: use '$PROG commit' to add this file permanently"
	    dotest recase-2si "$testcvs -q ci -mrecase" \
"Checking in FiLe;
$CVSROOT_DIRNAME/first-dir/FiLe,v  <--  FiLe
new revision: 1\.3; previous revision: 1\.2
done"
	  fi

	  # Now verify that a checkout will still work
	  cd ../..
	  mkdir 2; cd 2
	  dotest recase-3 "$testcvs -q co first-dir" \
"U first-dir/FiLe"

	  cd first-dir
	  # Prove that we can still get status and log information on
	  # conflicting case files (1 in Attic, one in parent).
	  if $remote; then
	    if $client_sensitive; then
	      file=file
	      fIlE=fIlE
	    else # client insensitive
	      # Because FiLe is present on a case insensitive client, it is the
	      # only one ever found and queried or altered.
	      file=FiLe
	      fIlE=FiLe
	    fi
	  else # ! $remote
	    file=file
	    fIlE=fIlE
	  fi
	  if $server_sensitive; then
	    if $client_sensitive; then
	      # Client finds Entry only for FiLe.  Others returned by server.
	      dotest recase-4sscs "$testcvs status file" \
"===================================================================
File: no file file		Status: Up-to-date

   Working revision:	No entry for file
   Repository revision:	1\.2	$CVSROOT_DIRNAME/first-dir/Attic/file,v"
	      dotest recase-5sscs "$testcvs log file" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/Attic/file,v
Working file: file
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	first: 1\.1
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: $username;  state: dead;  lines: +0 -0
rm
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
add
============================================================================="
	      dotest recase-6sscs "$testcvs status FiLe" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-7sscs "$testcvs log FiLe" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	    else # server sensitive && client insensitive
	      # Client finds same Entry for file & FiLe.
	      dotest recase-4ssci "$testcvs status file" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-5ssci "$testcvs log file" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	      dotest recase-6ss "$testcvs status FiLe" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-7ss "$testcvs log FiLe" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	    fi
	  else # server insensitive
	    # There is only one archive when the server is insensitive, but the
	    # printed file/archive name can vary.
	    dotest recase-4si "$testcvs status file" \
"===================================================================
File: $file             	Status: Up-to-date

   Working revision:	1\.3.*
   Repository revision:	1\.3	$CVSROOT_DIRNAME/first-dir/$file,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	    dotest recase-5si "$testcvs log file" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/$file,v
Working file: $file
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
	first: 1\.1
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.3
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;  lines: +1 -1
recase
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: $username;  state: dead;  lines: +0 -0
rm
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
add
============================================================================="
	    dotest recase-6si "$testcvs status FiLe" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.3.*
   Repository revision:	1\.3	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	    dotest recase-7si "$testcvs log FiLe" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.3
branch:
locks: strict
access list:
symbolic names:
	first: 1\.1
keyword substitution: kv
total revisions: 3;	selected revisions: 3
description:
----------------------------
revision 1\.3
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;  lines: +1 -1
recase
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: $username;  state: dead;  lines: +0 -0
rm
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
add
============================================================================="
	  fi

	  # And when the file does not exist on the client, we go with the
	  # client Entries match.
	  if $client_sensitive && $server_sensitive; then
	    dotest recase-8sscs "$testcvs status fIlE" \
"$PROG status: nothing known about fIlE
===================================================================
File: no file fIlE		Status: Unknown

   Working revision:	No entry for fIlE
   Repository revision:	No revision control file"
	  else # !$client_sensitive || !$server_sensitive
	    dotest recase-8anyi "$testcvs status fIlE" \
"===================================================================
File: $fIlE             	Status: Up-to-date

   Working revision:	1\.[0-9]*.*
   Repository revision:	1\.[0-9]*	$CVSROOT_DIRNAME/first-dir/$fIlE,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  fi

	  # and an update
	  if $server_sensitive; then
	    dotest recase-9ss "$testcvs -q up -rfirst" \
"$PROG update: FiLe is no longer in the repository
U file"

	    if $client_sensitive; then
	      dotest recase-10sscs "$testcvs -q up -A" \
"U FiLe
$PROG update: file is no longer in the repository"
	    else # client insensitive
	      # FIXCVS: This should remove the offending file first.
	      dotest_fail recase-10ssci "$testcvs -q up -A" \
"$PROG update: move away \./FiLe; it is in the way
C FiLe
$PROG update: file is no longer in the repository"

	      cd ..
	      rm -r first-dir
	      dotest recase-11ssci "$testcvs -q co first-dir" \
"U first-dir/FiLe"
	      cd first-dir
	    fi

	    #
	    # See what happens when cased names clash.
	    #

	    # Copy the archive
	    if test -n "$remotehost"; then
	      $CVS_RSH $remotehost "cp $CVSROOT_DIRNAME/first-dir/FiLe,v \
		$CVSROOT_DIRNAME/first-dir/FILE,v"
	    else
	      cp $CVSROOT_DIRNAME/first-dir/FiLe,v \
		$CVSROOT_DIRNAME/first-dir/FILE,v
	    fi

	    if $client_sensitive; then
	      dotest recase-12sscs "$testcvs -q up" "U FILE"
	    else # client insensitive
	      dotest_fail recase-12ssci "$testcvs -q up" \
"$PROG update: move away \./FILE; it is in the way
C FILE"
	    fi
	  else # server insensitive
	    dotest recase-9si "$testcvs -q up -rfirst" "U FiLe"
	    dotest recase-10si "$testcvs -q up -A" "U FiLe"
	  fi

	  # Prove that we can still get status and log information on
	  # conflicting case files (1 in Attic, two in parent).
	  if $server_sensitive; then
	    if $client_sensitive; then
	      # Client finds Entry only for FiLe.  Others returned by server.
	      dotest recase-13sscs "$testcvs status file" \
"===================================================================
File: no file file		Status: Up-to-date

   Working revision:	No entry for file
   Repository revision:	1\.2	$CVSROOT_DIRNAME/first-dir/Attic/file,v"
	    dotest recase-14sscs "$testcvs log file" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/Attic/file,v
Working file: file
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	first: 1\.1
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: $username;  state: dead;  lines: +0 -0
rm
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
add
============================================================================="
	    dotest recase-15sscs "$testcvs status FiLe" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-16sscs "$testcvs log FiLe" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	      dotest recase-17sscs "$testcvs status FILE" \
"===================================================================
File: FILE             	Status: Up-to-date

   Working revision:	1.1.*
   Repository revision:	1.1	${CVSROOT_DIRNAME}/first-dir/FILE,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-18sscs "$testcvs log FILE" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FILE,v
Working file: FILE
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	    else # $server_sensitive && !$client_sensitive
	      # Client finds same Entry for file & FiLe.
	      dotest recase-13ssci "$testcvs status file" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-16ssci "$testcvs log FiLe" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	      dotest recase-17ssci "$testcvs status FILE" \
"===================================================================
File: FiLe             	Status: Up-to-date

   Working revision:	1\.1.*
   Repository revision:	1\.1	$CVSROOT_DIRNAME/first-dir/FiLe,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	      dotest recase-18ssci "$testcvs log FILE" \
"
RCS file: $CVSROOT_DIRNAME/first-dir/FiLe,v
Working file: FiLe
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: $username;  state: Exp;
recase
============================================================================="
	    fi
	  else # !$server_sensitive
	    # Skip these when the server is case insensitive - nothing
	    # has changed since recase-[4-7]si
	    :
	  fi

	  if $client_sensitive && $server_sensitive; then
	    dotest recase-19sscs "$testcvs status fIlE" \
"$PROG status: nothing known about fIlE
===================================================================
File: no file fIlE		Status: Unknown

   Working revision:	No entry for fIlE
   Repository revision:	No revision control file"
	  else # !$client_sensitive || !$server_sensitive
	    dotest recase-19anyi "$testcvs status fIlE" \
"===================================================================
File: $fIlE             	Status: Up-to-date

   Working revision:	1\.[0-9]*.*
   Repository revision:	1\.[0-9]*	$CVSROOT_DIRNAME/first-dir/$fIlE,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)"
	  fi

	  # And last but not least, prove that a checkout is still possible.
	  cd ../..
	  mkdir 3; cd 3
	  if $server_sensitive; then
	    if $client_sensitive; then
	      dotest recase-20sscs "$testcvs -q co first-dir" \
"U first-dir/FILE
U first-dir/FiLe"
	    else # $server_senstive && !$client_sensitive
	      dotest_fail recase-20ssci "$testcvs -q co first-dir" \
"U first-dir/FILE
$PROG checkout: move away first-dir/FiLe; it is in the way
C first-dir/FiLe"
	    fi
	  else # !$server_sensitive
	    # Skip these since nothing has changed.
	    :
	  fi

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  cd ..
	  rm -r 1 2 3
	  if $server_sensitive && test -n "$remotehost"; then
	    # It is necessary to remove one of the case-conflicted files before
	    # recursively removing the rest under Cygwin on a Samba share or
	    # Samba returns a permission denied error due to its case
	    # confusion.
	    $CVS_RSH $remotehost "rm -f $CVSROOT_DIRNAME/first-dir/FILE,v"
	  fi
	  rm -rf $CVSROOT_DIRNAME/first-dir
	  ;;



	multiroot)
	  #
	  # set up two repositories
	  #

	  CVSROOT1_DIRNAME=${TESTDIR}/root1
	  CVSROOT2_DIRNAME=${TESTDIR}/root2
	  CVSROOT1=`newroot $CVSROOT1_DIRNAME`
	  CVSROOT2=`newroot $CVSROOT2_DIRNAME`
	  testcvs1="${testcvs} -d ${CVSROOT1}"
	  testcvs2="${testcvs} -d ${CVSROOT2}"

	  dotest multiroot-setup-1 "mkdir $CVSROOT1_DIRNAME $CVSROOT2_DIRNAME"
	  dotest multiroot-setup-2 "$testcvs -d$CVSROOT1_DIRNAME init"
	  dotest multiroot-setup-3 "$testcvs -d$CVSROOT2_DIRNAME init"

	  #
	  # create some directories in root1
	  #
	  mkdir 1; cd 1
	  dotest multiroot-setup-4 "${testcvs1} co -l ." \
"${PROG} checkout: Updating ."
	  mkdir mod1-1 mod1-2
	  dotest multiroot-setup-5 "${testcvs1} add mod1-1 mod1-2" \
"Directory ${CVSROOT1_DIRNAME}/mod1-1 added to the repository
Directory ${CVSROOT1_DIRNAME}/mod1-2 added to the repository"
	  echo file1-1 > mod1-1/file1-1
	  echo file1-2 > mod1-2/file1-2
	  dotest multiroot-setup-6 "${testcvs1} add mod1-1/file1-1 mod1-2/file1-2" \
"${PROG} add: scheduling file .mod1-1/file1-1. for addition
${PROG} add: scheduling file .mod1-2/file1-2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest multiroot-setup-7 "${testcvs1} commit -m is" \
"${PROG} [a-z]*: Examining \.
${PROG} [a-z]*: Examining mod1-1
${PROG} [a-z]*: Examining mod1-2
RCS file: ${CVSROOT1_DIRNAME}/mod1-1/file1-1,v
done
Checking in mod1-1/file1-1;
${CVSROOT1_DIRNAME}/mod1-1/file1-1,v  <--  file1-1
initial revision: 1.1
done
RCS file: ${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
done
Checking in mod1-2/file1-2;
${CVSROOT1_DIRNAME}/mod1-2/file1-2,v  <--  file1-2
initial revision: 1.1
done"
	  cd ..
	  rm -rf 1

	  #
	  # create some directories in root2
	  #
	  mkdir 1; cd 1
	  dotest multiroot-setup-8 "${testcvs2} co -l ." \
"${PROG} checkout: Updating ."
	  mkdir mod2-1 mod2-2
	  dotest multiroot-setup-9 "${testcvs2} add mod2-1 mod2-2" \
"Directory ${CVSROOT2_DIRNAME}/mod2-1 added to the repository
Directory ${CVSROOT2_DIRNAME}/mod2-2 added to the repository"
	  echo file2-1 > mod2-1/file2-1
	  echo file2-2 > mod2-2/file2-2
	  dotest multiroot-setup-6 "${testcvs2} add mod2-1/file2-1 mod2-2/file2-2" \
"${PROG} add: scheduling file .mod2-1/file2-1. for addition
${PROG} add: scheduling file .mod2-2/file2-2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest multiroot-setup-10 "${testcvs2} commit -m anyone" \
"${PROG} [a-z]*: Examining \.
${PROG} [a-z]*: Examining mod2-1
${PROG} [a-z]*: Examining mod2-2
RCS file: ${CVSROOT2_DIRNAME}/mod2-1/file2-1,v
done
Checking in mod2-1/file2-1;
${CVSROOT2_DIRNAME}/mod2-1/file2-1,v  <--  file2-1
initial revision: 1.1
done
RCS file: ${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
done
Checking in mod2-2/file2-2;
${CVSROOT2_DIRNAME}/mod2-2/file2-2,v  <--  file2-2
initial revision: 1.1
done"
	  cd ..
	  rm -rf 1

	  # check out a few directories, from simple/shallow to
	  # complex/deep
	  mkdir 1; cd 1

	  # OK, this case is kind of weird.  If we just run things from
	  # here, without CVS/Root, then CVS will contact the server
	  # mentioned in CVSROOT (which is irrelevant) which will print
	  # some messages.  Our workaround is to make sure we have a
	  # CVS/Root file at top level.  In the future, it is possible
	  # the best behavior will be to extend the existing behavior
	  # ("being called from a directory without CVS administration
	  # has always meant to process each of the sub-dirs") to also
	  # do that if there is no CVSROOT, CVS/Root, or -d at top level.
	  # 
	  # The local case could stumble through the tests without creating
	  # the top-level CVS/Root, but we create it for local and for
	  # remote to reduce special cases later in the test.
	  dotest multiroot-workaround "${testcvs1} -q co -l ." ""

	  dotest multiroot-setup-11 "${testcvs1} co mod1-1 mod1-2" \
"${PROG} checkout: Updating mod1-1
U mod1-1/file1-1
${PROG} checkout: Updating mod1-2
U mod1-2/file1-2"
	  dotest multiroot-setup-12 "${testcvs2} co mod2-1 mod2-2" \
"${PROG} checkout: Updating mod2-1
U mod2-1/file2-1
${PROG} checkout: Updating mod2-2
U mod2-2/file2-2"
	  cd mod1-2
	  dotest multiroot-setup-13 "${testcvs2} co mod2-2" \
"${PROG} checkout: Updating mod2-2
U mod2-2/file2-2"
	  cd ..
	  cd mod2-2
	  dotest multiroot-setup-14 "${testcvs1} co mod1-2" \
"${PROG} checkout: Updating mod1-2
U mod1-2/file1-2"
	  cd ..

	  #
	  # Make sure that the Root and Repository files contain the
	  # correct information.
	  #
	  dotest multiroot-cvsadm-1a "cat mod1-1/CVS/Root" "${CVSROOT1}"
	  dotest multiroot-cvsadm-1b "cat mod1-1/CVS/Repository" "mod1-1"
	  dotest multiroot-cvsadm-2a "cat mod2-1/CVS/Root" "${CVSROOT2}"
	  dotest multiroot-cvsadm-2b "cat mod2-1/CVS/Repository" "mod2-1"
	  dotest multiroot-cvsadm-3a "cat mod1-2/CVS/Root" "${CVSROOT1}"
	  dotest multiroot-cvsadm-3b "cat mod1-2/CVS/Repository" "mod1-2"
	  dotest multiroot-cvsadm-3c "cat mod1-2/mod2-2/CVS/Root" "${CVSROOT2}"
	  dotest multiroot-cvsadm-3d "cat mod1-2/mod2-2/CVS/Repository" "mod2-2"
	  dotest multiroot-cvsadm-4a "cat mod2-2/CVS/Root" "${CVSROOT2}"
	  dotest multiroot-cvsadm-4b "cat mod2-2/CVS/Repository" "mod2-2"
	  dotest multiroot-cvsadm-4c "cat mod2-2/mod1-2/CVS/Root" "${CVSROOT1}"
	  dotest multiroot-cvsadm-4d "cat mod2-2/mod1-2/CVS/Repository" "mod1-2"

	  #
	  # Start testing various cvs commands.  Begin with commands
	  # without extra arguments (e.g. "cvs update", "cvs diff",
	  # etc.
	  #

	  # Do at least one command with both CVSROOTs to make sure
	  # that there's not some kind of unexpected dependency on the
	  # choice of which CVSROOT is specified on the command line.

	  dotest multiroot-update-1a "${testcvs1} update" \
"${PROG} update: Updating \.
${PROG} update: Updating mod1-1
${PROG} update: Updating mod1-2
${PROG} update: Updating mod1-2/mod2-2
${PROG} update: cannot open directory ${CVSROOT1_DIRNAME}/mod2-2: No such file or directory
${PROG} update: skipping directory mod1-2/mod2-2
${PROG} update: Updating mod2-1
${PROG} update: cannot open directory ${CVSROOT1_DIRNAME}/mod2-1: No such file or directory
${PROG} update: skipping directory mod2-1
${PROG} update: Updating mod2-2
${PROG} update: cannot open directory ${CVSROOT1_DIRNAME}/mod2-2: No such file or directory
${PROG} update: skipping directory mod2-2"

	  # Same deal but with -d ${CVSROOT2}.
	  dotest multiroot-update-1b "${testcvs2} update" \
"${PROG} update: Updating \.
${PROG} update: Updating mod1-1
${PROG} update: cannot open directory ${CVSROOT2_DIRNAME}/mod1-1: No such file or directory
${PROG} update: skipping directory mod1-1
${PROG} update: Updating mod1-2
${PROG} update: cannot open directory ${CVSROOT2_DIRNAME}/mod1-2: No such file or directory
${PROG} update: skipping directory mod1-2
${PROG} update: Updating mod2-1
${PROG} update: Updating mod2-2
${PROG} update: Updating mod2-2/mod1-2
${PROG} update: cannot open directory ${CVSROOT2_DIRNAME}/mod1-2: No such file or directory
${PROG} update: skipping directory mod2-2/mod1-2"

	  # modify all files and do a diff

	  echo bobby >> mod1-1/file1-1
	  echo brown >> mod1-2/file1-2
	  echo goes >> mod2-1/file2-1
	  echo down >> mod2-2/file2-2

	  dotest_fail multiroot-diff-1 "${testcvs} diff" \
"${PROG} diff: Diffing \.
${PROG} diff: Diffing mod1-1
Index: mod1-1/file1-1
===================================================================
RCS file: ${TESTDIR}/root1/mod1-1/file1-1,v
retrieving revision 1\.1
diff -r1\.1 file1-1
1a2
> bobby
${PROG} diff: Diffing mod1-2
Index: mod1-2/file1-2
===================================================================
RCS file: ${TESTDIR}/root1/mod1-2/file1-2,v
retrieving revision 1\.1
diff -r1\.1 file1-2
1a2
> brown
${PROG} diff: Diffing mod2-2/mod1-2
${PROG} diff: Diffing mod1-2/mod2-2
${PROG} diff: Diffing mod2-1
Index: mod2-1/file2-1
===================================================================
RCS file: ${TESTDIR}/root2/mod2-1/file2-1,v
retrieving revision 1\.1
diff -r1\.1 file2-1
1a2
> goes
${PROG} diff: Diffing mod2-2
Index: mod2-2/file2-2
===================================================================
RCS file: ${TESTDIR}/root2/mod2-2/file2-2,v
retrieving revision 1\.1
diff -r1\.1 file2-2
1a2
> down" \
"${PROG} diff: Diffing \.
${PROG} diff: Diffing mod1-1
Index: mod1-1/file1-1
===================================================================
RCS file: ${TESTDIR}/root1/mod1-1/file1-1,v
retrieving revision 1\.1
diff -r1\.1 file1-1
1a2
> bobby
${PROG} diff: Diffing mod1-2
Index: mod1-2/file1-2
===================================================================
RCS file: ${TESTDIR}/root1/mod1-2/file1-2,v
retrieving revision 1\.1
diff -r1\.1 file1-2
1a2
> brown
${PROG} diff: Diffing mod2-2
${PROG} diff: Diffing mod2-2/mod1-2
${PROG} diff: Diffing mod1-2
${PROG} diff: Diffing mod1-2/mod2-2
${PROG} diff: Diffing mod2-1
Index: mod2-1/file2-1
===================================================================
RCS file: ${TESTDIR}/root2/mod2-1/file2-1,v
retrieving revision 1\.1
diff -r1\.1 file2-1
1a2
> goes
${PROG} diff: Diffing mod2-2
Index: mod2-2/file2-2
===================================================================
RCS file: ${TESTDIR}/root2/mod2-2/file2-2,v
retrieving revision 1\.1
diff -r1\.1 file2-2
1a2
> down"

	  dotest multiroot-commit-1 "${testcvs} commit -m actually" \
"${PROG} [a-z]*: Examining \.
${PROG} [a-z]*: Examining mod1-1
${PROG} [a-z]*: Examining mod1-2
${PROG} [a-z]*: Examining mod2-2/mod1-2
Checking in mod1-1/file1-1;
${TESTDIR}/root1/mod1-1/file1-1,v  <--  file1-1
new revision: 1.2; previous revision: 1.1
done
Checking in mod1-2/file1-2;
${TESTDIR}/root1/mod1-2/file1-2,v  <--  file1-2
new revision: 1.2; previous revision: 1.1
done
${PROG} [a-z]*: Examining mod1-2/mod2-2
${PROG} [a-z]*: Examining mod2-1
${PROG} [a-z]*: Examining mod2-2
Checking in mod2-1/file2-1;
${TESTDIR}/root2/mod2-1/file2-1,v  <--  file2-1
new revision: 1.2; previous revision: 1.1
done
Checking in mod2-2/file2-2;
${TESTDIR}/root2/mod2-2/file2-2,v  <--  file2-2
new revision: 1.2; previous revision: 1.1
done"

	  dotest multiroot-update-2 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} [a-z]*: Updating mod1-1
${PROG} [a-z]*: Updating mod1-2
${PROG} [a-z]*: Updating mod2-2/mod1-2
U mod2-2/mod1-2/file1-2
${PROG} [a-z]*: Updating mod1-2/mod2-2
U mod1-2/mod2-2/file2-2
${PROG} update: Updating mod2-1
${PROG} update: Updating mod2-2" \
"${PROG} update: Updating \.
${PROG} update: Updating mod1-1
${PROG} update: Updating mod1-2
${PROG} update: Updating mod2-2
${PROG} update: Updating mod2-2/mod1-2
P mod2-2/mod1-2/file1-2
${PROG} update: Updating mod1-2
${PROG} update: Updating mod1-2/mod2-2
P mod1-2/mod2-2/file2-2
${PROG} update: Updating mod2-1
${PROG} update: Updating mod2-2"

	  dotest multiroot-tag-1 "${testcvs} tag cattle" \
"${PROG} tag: Tagging \.
${PROG} tag: Tagging mod1-1
T mod1-1/file1-1
${PROG} tag: Tagging mod1-2
T mod1-2/file1-2
${PROG} tag: Tagging mod2-2/mod1-2
${PROG} tag: Tagging mod1-2/mod2-2
T mod1-2/mod2-2/file2-2
${PROG} tag: Tagging mod2-1
T mod2-1/file2-1
${PROG} tag: Tagging mod2-2" \
"${PROG} tag: Tagging \.
${PROG} tag: Tagging mod1-1
T mod1-1/file1-1
${PROG} tag: Tagging mod1-2
T mod1-2/file1-2
${PROG} tag: Tagging mod2-2
${PROG} tag: Tagging mod2-2/mod1-2
${PROG} tag: Tagging mod1-2
${PROG} tag: Tagging mod1-2/mod2-2
T mod1-2/mod2-2/file2-2
${PROG} tag: Tagging mod2-1
T mod2-1/file2-1
${PROG} tag: Tagging mod2-2"

	  echo anotherfile1-1 > mod1-1/anotherfile1-1
	  echo anotherfile2-1 > mod2-1/anotherfile2-1
	  echo anotherfile1-2 > mod2-2/mod1-2/anotherfile1-2
	  echo anotherfile2-2 > mod1-2/mod2-2/anotherfile2-2

	  if $remote; then
	    cd mod1-1
	    dotest multiroot-add-1ar "${testcvs} add anotherfile1-1" \
"${PROG} add: scheduling file .anotherfile1-1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	    cd ../mod2-1
	    dotest multiroot-add-1br "${testcvs} add anotherfile2-1" \
"${PROG} add: scheduling file .anotherfile2-1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	    cd ../mod2-2/mod1-2
	    dotest multiroot-add-1cr "${testcvs} add anotherfile1-2" \
"${PROG} add: scheduling file .anotherfile1-2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	    cd ../../mod1-2/mod2-2
	    dotest multiroot-add-1dr "${testcvs} add anotherfile2-2" \
"${PROG} add: scheduling file .anotherfile2-2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	    cd ../..
          else
	    dotest multiroot-add-1 "${testcvs} add mod1-1/anotherfile1-1 mod2-1/anotherfile2-1 mod2-2/mod1-2/anotherfile1-2 mod1-2/mod2-2/anotherfile2-2" \
"${PROG} add: scheduling file .mod1-1/anotherfile1-1. for addition
${PROG} add: scheduling file .mod2-1/anotherfile2-1. for addition
${PROG} add: scheduling file .mod2-2/mod1-2/anotherfile1-2. for addition
${PROG} add: scheduling file .mod1-2/mod2-2/anotherfile2-2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
          fi

	  dotest multiroot-status-1 "${testcvs} status -v" \
"${PROG} status: Examining \.
${PROG} status: Examining mod1-1
===================================================================
File: anotherfile1-1   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file1-1          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT1_DIRNAME}/mod1-1/file1-1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod1-2
===================================================================
File: file1-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod2-2/mod1-2
===================================================================
File: anotherfile1-2   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file1-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod1-2/mod2-2
===================================================================
File: anotherfile2-2   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file2-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod2-1
===================================================================
File: anotherfile2-1   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file2-1          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT2_DIRNAME}/mod2-1/file2-1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod2-2
===================================================================
File: file2-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)" \
"${PROG} status: Examining \.
${PROG} status: Examining mod1-1
===================================================================
File: anotherfile1-1   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file1-1          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT1_DIRNAME}/mod1-1/file1-1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod1-2
===================================================================
File: file1-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod2-2
${PROG} status: Examining mod2-2/mod1-2
===================================================================
File: anotherfile1-2   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file1-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod1-2
${PROG} status: Examining mod1-2/mod2-2
===================================================================
File: anotherfile2-2   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file2-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod2-1
===================================================================
File: anotherfile2-1   	Status: Locally Added

   Working revision:	New file!
   Repository revision:	No revision control file
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

===================================================================
File: file2-1          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT2_DIRNAME}/mod2-1/file2-1,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)

${PROG} status: Examining mod2-2
===================================================================
File: file2-2          	Status: Up-to-date

   Working revision:	1\.2.*
   Repository revision:	1\.2	${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
   Sticky Tag:		(none)
   Sticky Date:		(none)
   Sticky Options:	(none)

   Existing Tags:
	cattle                   	(revision: 1\.2)"

	  dotest multiroot-commit-2 "${testcvs} commit -m reading" \
"${PROG} [a-z]*: Examining \.
${PROG} [a-z]*: Examining mod1-1
${PROG} [a-z]*: Examining mod1-2
${PROG} [a-z]*: Examining mod2-2/mod1-2
RCS file: ${CVSROOT1_DIRNAME}/mod1-1/anotherfile1-1,v
done
Checking in mod1-1/anotherfile1-1;
${CVSROOT1_DIRNAME}/mod1-1/anotherfile1-1,v  <--  anotherfile1-1
initial revision: 1\.1
done
RCS file: ${CVSROOT1_DIRNAME}/mod1-2/anotherfile1-2,v
done
Checking in mod2-2/mod1-2/anotherfile1-2;
${CVSROOT1_DIRNAME}/mod1-2/anotherfile1-2,v  <--  anotherfile1-2
initial revision: 1\.1
done
${PROG} [a-z]*: Examining mod1-2/mod2-2
${PROG} [a-z]*: Examining mod2-1
${PROG} [a-z]*: Examining mod2-2
RCS file: ${CVSROOT2_DIRNAME}/mod2-2/anotherfile2-2,v
done
Checking in mod1-2/mod2-2/anotherfile2-2;
${CVSROOT2_DIRNAME}/mod2-2/anotherfile2-2,v  <--  anotherfile2-2
initial revision: 1\.1
done
RCS file: ${CVSROOT2_DIRNAME}/mod2-1/anotherfile2-1,v
done
Checking in mod2-1/anotherfile2-1;
${CVSROOT2_DIRNAME}/mod2-1/anotherfile2-1,v  <--  anotherfile2-1
initial revision: 1\.1
done"

	  dotest multiroot-update-3 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} [a-z]*: Updating mod1-1
${PROG} [a-z]*: Updating mod1-2
U mod1-2/anotherfile1-2
${PROG} [a-z]*: Updating mod2-2/mod1-2
${PROG} [a-z]*: Updating mod1-2/mod2-2
${PROG} [a-z]*: Updating mod2-1
${PROG} [a-z]*: Updating mod2-2
U mod2-2/anotherfile2-2" \
"${PROG} update: Updating \.
${PROG} update: Updating mod1-1
${PROG} update: Updating mod1-2
U mod1-2/anotherfile1-2
${PROG} update: Updating mod2-2
${PROG} update: Updating mod2-2/mod1-2
${PROG} update: Updating mod1-2
${PROG} update: Updating mod1-2/mod2-2
${PROG} update: Updating mod2-1
${PROG} update: Updating mod2-2
U mod2-2/anotherfile2-2"

	  dotest multiroot-log-1 "${testcvs} log" \
"${PROG} log: Logging \.
${PROG} log: Logging mod1-1

RCS file: ${CVSROOT1_DIRNAME}/mod1-1/anotherfile1-1,v
Working file: mod1-1/anotherfile1-1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT1_DIRNAME}/mod1-1/file1-1,v
Working file: mod1-1/file1-1
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
is
=============================================================================
${PROG} log: Logging mod1-2

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/anotherfile1-2,v
Working file: mod1-2/anotherfile1-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
Working file: mod1-2/file1-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
is
=============================================================================
${PROG} log: Logging mod2-2/mod1-2

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/anotherfile1-2,v
Working file: mod2-2/mod1-2/anotherfile1-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
Working file: mod2-2/mod1-2/file1-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
is
=============================================================================
${PROG} log: Logging mod1-2/mod2-2

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/anotherfile2-2,v
Working file: mod1-2/mod2-2/anotherfile2-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
Working file: mod1-2/mod2-2/file2-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
anyone
=============================================================================
${PROG} log: Logging mod2-1

RCS file: ${CVSROOT2_DIRNAME}/mod2-1/anotherfile2-1,v
Working file: mod2-1/anotherfile2-1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT2_DIRNAME}/mod2-1/file2-1,v
Working file: mod2-1/file2-1
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
anyone
=============================================================================
${PROG} log: Logging mod2-2

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/anotherfile2-2,v
Working file: mod2-2/anotherfile2-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
Working file: mod2-2/file2-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
anyone
=============================================================================" \
"${PROG} log: Logging \.
${PROG} log: Logging mod1-1

RCS file: ${CVSROOT1_DIRNAME}/mod1-1/anotherfile1-1,v
Working file: mod1-1/anotherfile1-1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT1_DIRNAME}/mod1-1/file1-1,v
Working file: mod1-1/file1-1
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
is
=============================================================================
${PROG} log: Logging mod1-2

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/anotherfile1-2,v
Working file: mod1-2/anotherfile1-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
Working file: mod1-2/file1-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
is
=============================================================================
${PROG} log: Logging mod2-2
${PROG} log: Logging mod2-2/mod1-2

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/anotherfile1-2,v
Working file: mod2-2/mod1-2/anotherfile1-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT1_DIRNAME}/mod1-2/file1-2,v
Working file: mod2-2/mod1-2/file1-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
is
=============================================================================
${PROG} log: Logging mod1-2
${PROG} log: Logging mod1-2/mod2-2

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/anotherfile2-2,v
Working file: mod1-2/mod2-2/anotherfile2-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
Working file: mod1-2/mod2-2/file2-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
anyone
=============================================================================
${PROG} log: Logging mod2-1

RCS file: ${CVSROOT2_DIRNAME}/mod2-1/anotherfile2-1,v
Working file: mod2-1/anotherfile2-1
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT2_DIRNAME}/mod2-1/file2-1,v
Working file: mod2-1/file2-1
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
anyone
=============================================================================
${PROG} log: Logging mod2-2

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/anotherfile2-2,v
Working file: mod2-2/anotherfile2-2
head: 1\.1
branch:
locks: strict
access list:
symbolic names:
keyword substitution: kv
total revisions: 1;	selected revisions: 1
description:
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
reading
=============================================================================

RCS file: ${CVSROOT2_DIRNAME}/mod2-2/file2-2,v
Working file: mod2-2/file2-2
head: 1\.2
branch:
locks: strict
access list:
symbolic names:
	cattle: 1\.2
keyword substitution: kv
total revisions: 2;	selected revisions: 2
description:
----------------------------
revision 1\.2
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;  lines: ${PLUS}1 -0
actually
----------------------------
revision 1\.1
date: [0-9/]* [0-9:]*;  author: ${username};  state: Exp;
anyone
============================================================================="


	  # After the simple cases, let's execute some commands which
	  # refer to parts of our checked-out tree (e.g. "cvs update
	  # mod1-1 mod2-2")

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  # clean up after ourselves
	  cd ..
	  rm -r 1

	  # clean up our repositories
	  rm -rf root1 root2
	  ;;

	multiroot2)
	  # More multiroot tests.  In particular, nested directories.

	  CVSROOT1_DIRNAME=${TESTDIR}/root1
	  CVSROOT2_DIRNAME=${TESTDIR}/root2
	  CVSROOT1=`newroot $CVSROOT1_DIRNAME`
	  CVSROOT2=`newroot $CVSROOT2_DIRNAME`

	  dotest multiroot2-1 "$testcvs -d$CVSROOT1_DIRNAME init"
	  dotest multiroot2-2 "$testcvs -d$CVSROOT2_DIRNAME init"

	  mkdir imp-dir; cd imp-dir
	  echo file1 >file1
	  mkdir sdir
	  echo sfile >sdir/sfile
	  mkdir sdir/ssdir
	  echo ssfile >sdir/ssdir/ssfile
	  dotest_sort multiroot2-3 \
"${testcvs} -d ${CVSROOT1} import -m import-to-root1 dir1 vend rel" "

N dir1/file1
N dir1/sdir/sfile
N dir1/sdir/ssdir/ssfile
No conflicts created by this import
${PROG} import: Importing ${TESTDIR}/root1/dir1/sdir
${PROG} import: Importing ${TESTDIR}/root1/dir1/sdir/ssdir"
	  cd sdir
	  dotest_sort multiroot2-4 \
"${testcvs} -d ${CVSROOT2} import -m import-to-root2 sdir vend2 rel2" "

N sdir/sfile
N sdir/ssdir/ssfile
No conflicts created by this import
${PROG} import: Importing ${TESTDIR}/root2/sdir/ssdir"
	  cd ../..

	  mkdir 1; cd 1
	  # Get TopLevelAdmin-like behavior.
	  dotest multiroot2-5 "${testcvs} -d ${CVSROOT1} -q co -l ."
	  dotest multiroot2-5 "${testcvs} -d ${CVSROOT1} -q co dir1" \
"U dir1/file1
U dir1/sdir/sfile
U dir1/sdir/ssdir/ssfile"
	  cd dir1
	  dotest multiroot2-6 "${testcvs} -Q release -d sdir" ""
	  dotest multiroot2-7 "${testcvs} -d ${CVSROOT2} -q co sdir" \
"U sdir/sfile
U sdir/ssdir/ssfile"
	  cd ..
	  # This has one subtle effect - it deals with Entries.Log
	  # so that the next test doesn't get trace messages for
	  # Entries.Log
	  dotest multiroot2-8 "${testcvs} update" \
"${PROG} update: Updating \.
${PROG} update: Updating dir1
${PROG} update: Updating dir1/sdir
${PROG} update: Updating dir1/sdir/ssdir" \
"${PROG} update: Updating \.
${PROG} update: Updating dir1
${PROG} update: Updating dir1
${PROG} update: Updating dir1/sdir
${PROG} update: Updating dir1/sdir/ssdir"
	  # Two reasons we don't run this on the server: (1) the server
	  # also prints some trace messages, and (2) the server trace
	  # messages are subject to out-of-order bugs (this one is hard
	  # to work around).
	  if $remote; then :; else
	    dotest multiroot2-9a "${testcvs} -t update" \
" *-> main loop with CVSROOT=${TESTDIR}/root1
${PROG} update: Updating \.
 *-> Reader_Lock(${TESTDIR}/root1)
 *-> Lock_Cleanup()
${PROG} update: Updating dir1
 *-> Reader_Lock(${TESTDIR}/root1/dir1)
 *-> Lock_Cleanup()
 *-> main loop with CVSROOT=${TESTDIR}/root2
${PROG} update: Updating dir1/sdir
 *-> Reader_Lock(${TESTDIR}/root2/sdir)
 *-> Lock_Cleanup()
${PROG} update: Updating dir1/sdir/ssdir
 *-> Reader_Lock(${TESTDIR}/root2/sdir/ssdir)
 *-> Lock_Cleanup()
 *-> Lock_Cleanup()"
	  fi

	  dotest multiroot2-9 "${testcvs} -q tag tag1" \
"T dir1/file1
T dir1/sdir/sfile
T dir1/sdir/ssdir/ssfile"
	  echo "change it" >>dir1/file1
	  echo "change him too" >>dir1/sdir/sfile
	  dotest multiroot2-10 "${testcvs} -q ci -m modify" \
"Checking in dir1/file1;
${TESTDIR}/root1/dir1/file1,v  <--  file1
new revision: 1\.2; previous revision: 1\.1
done
Checking in dir1/sdir/sfile;
${TESTDIR}/root2/sdir/sfile,v  <--  sfile
new revision: 1\.2; previous revision: 1\.1
done"
	  dotest multiroot2-11 "${testcvs} -q tag tag2" \
"T dir1/file1
T dir1/sdir/sfile
T dir1/sdir/ssdir/ssfile"
	  dotest_fail multiroot2-12 \
"${testcvs} -q diff -u -r tag1 -r tag2" \
"Index: dir1/file1
===================================================================
RCS file: ${TESTDIR}/root1/dir1/file1,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.2
diff -u -r1\.1\.1\.1 -r1\.2
--- dir1/file1	${RFCDATE}	1\.1\.1\.1
${PLUS}${PLUS}${PLUS} dir1/file1	${RFCDATE}	1\.2
@@ -1 ${PLUS}1,2 @@
 file1
${PLUS}change it
Index: dir1/sdir/sfile
===================================================================
RCS file: ${TESTDIR}/root2/sdir/sfile,v
retrieving revision 1\.1\.1\.1
retrieving revision 1\.2
diff -u -r1\.1\.1\.1 -r1\.2
--- dir1/sdir/sfile	${RFCDATE}	1\.1\.1\.1
${PLUS}${PLUS}${PLUS} dir1/sdir/sfile	${RFCDATE}	1\.2
@@ -1 ${PLUS}1,2 @@
 sfile
${PLUS}change him too"

	  if $keep; then
	    echo Keeping ${TESTDIR} and exiting due to --keep
	    exit 0
	  fi

	  # clean up after ourselves
	  cd ..
	  rm -r imp-dir 1

	  # clean up our repositories
	  rm -rf root1 root2
	  ;;

	multiroot3)
	  # More multiroot tests.  Directories are side-by-side, not nested.
	  # Not drastically different from multiroot but it covers somewhat
	  # different stuff.

	  CVSROOT1=`newroot ${TESTDIR}/root1`
	  CVSROOT2=`newroot ${TESTDIR}/root2`

	  mkdir 1; cd 1
	  dotest multiroot3-1 "$testcvs -d$TESTDIR/root1 init"
	  dotest multiroot3-2 "${testcvs} -d ${CVSROOT1} -q co -l ." ""
	  mkdir dir1
	  dotest multiroot3-3 "${testcvs} add dir1" \
"Directory ${TESTDIR}/root1/dir1 added to the repository"
	  dotest multiroot3-4 "$testcvs -d$TESTDIR/root2 init"
	  rm -r CVS
	  dotest multiroot3-5 "${testcvs} -d ${CVSROOT2} -q co -l ." ""
	  mkdir dir2

	  # OK, the problem is that CVS/Entries doesn't look quite right,
	  # I suppose because of the "rm -r".
	  # For local this fixes it up.
	  dotest multiroot3-6 "${testcvs} -d ${CVSROOT1} -q co dir1" ""
	  if $remote; then
	    # For remote that doesn't do it.  Use the quick and dirty fix.
	    echo "D/dir1////" >CVS/Entries
	    echo "D/dir2////" >>CVS/Entries
	  fi

	  dotest multiroot3-7 "${testcvs} add dir2" \
"Directory ${TESTDIR}/root2/dir2 added to the repository"

	  touch dir1/file1 dir2/file2
	  if $remote; then
	    # Trying to add them both in one command doesn't work,
	    # because add.c doesn't do multiroot (it doesn't use recurse.c).
	    # Furthermore, it can't deal with the parent directory
	    # having a different root from the child, hence the cd.
	    cd dir1
	    dotest multiroot3-8 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	    cd ..
	    dotest multiroot3-8a "${testcvs} add dir2/file2" \
"${PROG} add: scheduling file .dir2/file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  else
	    dotest multiroot3-8 "${testcvs} add dir1/file1 dir2/file2" \
"${PROG} add: scheduling file .dir1/file1. for addition
${PROG} add: scheduling file .dir2/file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  fi

	  dotest multiroot3-9 "${testcvs} -q ci -m add-them" \
"RCS file: ${TESTDIR}/root2/dir2/file2,v
done
Checking in dir2/file2;
${TESTDIR}/root2/dir2/file2,v  <--  file2
initial revision: 1\.1
done
RCS file: ${TESTDIR}/root1/dir1/file1,v
done
Checking in dir1/file1;
${TESTDIR}/root1/dir1/file1,v  <--  file1
initial revision: 1\.1
done"

	  # That this is an error is good - we are asking CVS to do
	  # something which doesn't make sense.
	  dotest_fail multiroot3-10 \
"${testcvs} -q -d ${CVSROOT1} diff dir1/file1 dir2/file2" \
"${PROG} diff: failed to create lock directory for .${TESTDIR}/root1/dir2' (${TESTDIR}/root1/dir2/#cvs.lock): No such file or directory
${PROG} diff: failed to obtain dir lock in repository .${TESTDIR}/root1/dir2'
${PROG} \[diff aborted\]: read lock failed - giving up"

	  # This one is supposed to work.
	  dotest multiroot3-11 "${testcvs} -q diff dir1/file1 dir2/file2" ""

	  # make sure we can't access across repositories
	  mkdir 1a
	  cd 1a
	  dotest_fail multiroot3-12 \
"$testcvs -d $CVSROOT1 -q co ../root2/dir2" \
"$PROG \[checkout aborted\]: up-level in module reference (\`..') invalid: \`\.\./root2/dir2'\." \
"$PROG \[server aborted\]: up-level in module reference (\`..') invalid: \`\.\./root2/dir2'\.
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"
	  dotest_fail multiroot3-13 \
"$testcvs -d $CVSROOT2 -q co ../root1/dir1" \
"$PROG \[checkout aborted\]: up-level in module reference (\`..') invalid: \`\.\./root1/dir1'\." \
"$PROG \[server aborted\]: up-level in module reference (\`..') invalid: \`\.\./root1/dir1'\.
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"
	  dotest_fail multiroot3-14 \
"$testcvs -d $CVSROOT1 -q co ./../root2/dir2" \
"$PROG \[checkout aborted\]: up-level in module reference (\`..') invalid: \`\./\.\./root2/dir2'\." \
"$PROG \[server aborted\]: up-level in module reference (\`..') invalid: \`\./\.\./root2/dir2'\.
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"
	  dotest_fail multiroot3-15 \
"$testcvs -d $CVSROOT2 -q co ./../root1/dir1" \
"$PROG \[checkout aborted\]: up-level in module reference (\`..') invalid: \`\./\.\./root1/dir1'\." \
"$PROG \[server aborted\]: up-level in module reference (\`..') invalid: \`\./\.\./root1/dir1'\.
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"
	  dotest_fail multiroot3-16 \
"$testcvs -d $CVSROOT1 -q co -p ../root2/dir2" \
"$PROG \[checkout aborted\]: up-level in module reference (\`..') invalid: \`\.\./root2/dir2'\." \
"$PROG \[server aborted\]: up-level in module reference (\`..') invalid: \`\.\./root2/dir2'\.
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"
	  dotest_fail multiroot3-17 \
"$testcvs -d $CVSROOT1 -q co -p ./../root1/dir1" \
"$PROG \[checkout aborted\]: up-level in module reference (\`..') invalid: \`\./\.\./root1/dir1'\." \
"$PROG \[server aborted\]: up-level in module reference (\`..') invalid: \`\./\.\./root1/dir1'\.
$PROG \[checkout aborted\]: end of file from server (consult above messages if any)"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  cd ../..

	  rm -r 1
	  rm -rf ${TESTDIR}/root1 ${TESTDIR}/root2
	  unset CVSROOT1
	  unset CVSROOT2
	  ;;

	multiroot4)
	  # More multiroot tests, in particular we have two roots with
	  # similarly-named directories and we try to see that CVS can
	  # keep them separate.

	  CVSROOT1=`newroot ${TESTDIR}/root1`
	  CVSROOT2=`newroot ${TESTDIR}/root2`

	  mkdir 1; cd 1
	  dotest multiroot4-1 "$testcvs -d$TESTDIR/root1 init"
	  dotest multiroot4-2 "${testcvs} -d ${CVSROOT1} -q co -l ." ""
	  mkdir dircom
	  dotest multiroot4-3 "${testcvs} add dircom" \
"Directory ${TESTDIR}/root1/dircom added to the repository"
	  cd dircom
	  touch file1
	  dotest multiroot4-4 "${testcvs} add file1" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest multiroot4-5 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/root1/dircom/file1,v
done
Checking in file1;
${TESTDIR}/root1/dircom/file1,v  <--  file1
initial revision: 1\.1
done"
	  cd ../..
	  mkdir 2; cd 2
	  dotest multiroot4-6 "$testcvs -d$TESTDIR/root2 init"
	  dotest multiroot4-7 "${testcvs} -d ${CVSROOT2} -q co -l ." ""
	  mkdir dircom
	  dotest multiroot4-8 "${testcvs} add dircom" \
"Directory ${TESTDIR}/root2/dircom added to the repository"
	  cd dircom
	  touch file2
	  dotest multiroot4-9 "${testcvs} add file2" \
"${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add this file permanently"
	  dotest multiroot4-10 "${testcvs} -q ci -m add" \
"RCS file: ${TESTDIR}/root2/dircom/file2,v
done
Checking in file2;
${TESTDIR}/root2/dircom/file2,v  <--  file2
initial revision: 1\.1
done"

	  cd ../..
	  cd 1/dircom
	  # This may look contrived; the real world example which inspired
	  # it was that a user was changing from local to remote.  Cases
	  # like switching servers (among those mounting the same
	  # repository) and so on would also look the same.
	  mkdir sdir2
	  dotest multiroot4-11 "${testcvs} -d ${CVSROOT2} add sdir2" \
"Directory ${TESTDIR}/root2/dircom/sdir2 added to the repository"

	  dotest multiroot4-12 "${testcvs} -q update" ""
	  cd ..
	  dotest multiroot4-13 "${testcvs} -q update dircom" ""
	  cd ..

	  rm -r 1 2
	  rm -rf ${TESTDIR}/root1 ${TESTDIR}/root2
	  unset CVSROOT1
	  unset CVSROOT2
	  ;;

	rmroot)
	  # When the Entries/Root file is removed from an existing
	  # workspace, CVS should assume $CVSROOT instead
	  #
	  # Right now only checking that CVS exits normally on an
	  # update once CVS/Root is deleted
	  #
	  # There was a time when this would core dump when run in
	  # client/server mode

	  mkdir 1; cd 1
	  dotest rmroot-setup-1 "${testcvs} -q co -l ." ''
	  mkdir first-dir
	  dotest rmroot-setup-2 "${testcvs} add first-dir" \
"Directory ${CVSROOT_DIRNAME}/first-dir added to the repository"
          cd first-dir
	  touch file1 file2
	  dotest rmroot-setup-3 "${testcvs} add file1 file2" \
"${PROG} add: scheduling file .file1. for addition
${PROG} add: scheduling file .file2. for addition
${PROG} add: use .${PROG} commit. to add these files permanently"
	  dotest rmroot-setup-4 "${testcvs} -q commit -minit" \
"RCS file: ${CVSROOT_DIRNAME}/first-dir/file1,v
done
Checking in file1;
${CVSROOT_DIRNAME}/first-dir/file1,v  <--  file1
initial revision: 1\.1
done
RCS file: ${CVSROOT_DIRNAME}/first-dir/file2,v
done
Checking in file2;
${CVSROOT_DIRNAME}/first-dir/file2,v  <--  file2
initial revision: 1\.1
done"
	  rm CVS/Root
	  dotest rmroot-1 "${testcvs} -q update" ''

	  cd ../..
	  rm -rf 1
	  ;;

	reposmv)
	  # More tests of repositories and specifying them.
	  # Similar to crerepos but that test is probably getting big
	  # enough.
	  CVSROOT1=`newroot ${TESTDIR}/root1`
	  CVSROOT_MOVED=`newroot ${TESTDIR}/root-moved`

	  dotest reposmv-setup-1 "$testcvs -d$TESTDIR/root1 init"
	  mkdir imp-dir; cd imp-dir
	  echo file1 >file1
	  dotest reposmv-setup-2 \
"${testcvs} -d ${CVSROOT1} import -m add dir1 vendor release" \
"N dir1/file1

No conflicts created by this import"
	  cd ..

	  mkdir 1; cd 1
	  dotest reposmv-1 "${testcvs} -d ${CVSROOT1} -Q co dir1" ""
	  mv ${TESTDIR}/root1 ${TESTDIR}/root-moved
	  cd dir1

	  # If we didn't have a relative repository, get one now.
	  dotest reposmv-1a "cat CVS/Repository" \
"${TESTDIR}/root1/dir1" "dir1"
	  echo dir1 >CVS/Repository

	  # There were some duplicated warnings and such; only test
	  # for the part of the error message which makes sense.
	  # Bug: "skipping directory " without filename.
	  if $remote; then
	    dotest_fail reposmv-2r "${testcvs} update" \
"Cannot access ${TESTDIR}/root1/CVSROOT
No such file or directory"
	  else
	    dotest reposmv-2 "${testcvs} update" "${DOTSTAR}
${PROG} update: ignoring CVS/Root because it specifies a non-existent repository ${TESTDIR}/root1
${PROG} update: cannot open directory ${CVSROOT_DIRNAME}/dir1: No such file or directory
${PROG} update: skipping directory "
	  fi

	  # CVS/Root overrides $CVSROOT
	  if $remote; then
	    CVSROOT_save=${CVSROOT}
	    CVSROOT=:fork:${TESTDIR}/root-moved; export CVSROOT
	    dotest_fail reposmv-3r "${testcvs} update" \
"Cannot access ${TESTDIR}/root1/CVSROOT
No such file or directory"
	    CVSROOT=${CVSROOT_save}; export CVSROOT
	  else
	    CVSROOT_save=${CVSROOT}
	    CVSROOT=${TESTDIR}/root-moved; export CVSROOT
	    dotest reposmv-3 "${testcvs} update" \
"${DOTSTAR}
${PROG} update: ignoring CVS/Root because it specifies a non-existent repository ${TESTDIR}/root1
${PROG} update: Updating \.
${DOTSTAR}"
	    CVSROOT=${CVSROOT_save}; export CVSROOT
	  fi

	  if $remote; then
	    CVSROOT_save=${CVSROOT}
	    CVSROOT=:fork:${TESTDIR}/root-none; export CVSROOT
	    dotest_fail reposmv-4 "${testcvs} update" \
"Cannot access ${TESTDIR}/root1/CVSROOT
No such file or directory"
	    CVSROOT=${CVSROOT_save}; export CVSROOT
	  else
	    # CVS/Root doesn't seem to quite completely override $CVSROOT
	    # Bug?  Not necessarily a big deal if it only affects error
	    # messages.
	    CVSROOT_save=${CVSROOT}
	    CVSROOT=${TESTDIR}/root-none; export CVSROOT
	    dotest_fail reposmv-4 "${testcvs} update" \
"${PROG} update: in directory \.:
${PROG} update: ignoring CVS/Root because it specifies a non-existent repository ${TESTDIR}/root1
${PROG} \[update aborted\]: ${TESTDIR}/root-none/CVSROOT: No such file or directory"
	    CVSROOT=${CVSROOT_save}; export CVSROOT
	  fi

	  # -d overrides CVS/Root
	  # 
	  # Oddly enough, with CVS 1.10 I think this didn't work for
	  # local (that is, it would appear that CVS/Root would not
	  # get used, but would produce an error if it didn't exist).
	  dotest reposmv-5 "${testcvs} -d ${CVSROOT_MOVED} update" \
"${PROG} update: Updating \."

	  # TODO: could also test various other things, like what if the
	  # user removes CVS/Root (which is legit).  Or another set of
	  # tests would be if both repositories exist but we want to make
	  # sure that CVS is using the correct one.

	  cd ../..
	  rm -r imp-dir 1
	  rm -rf root1 root2
	  unset CVSROOT1
	  ;;

	pserver)
	  # Test basic pserver functionality.
	  if $remote; then
   	    # First set SystemAuth=no.  Not really necessary, I don't
	    # think, but somehow it seems like the clean thing for
	    # the testsuite.
	    mkdir 1; cd 1
	    dotest pserver-1 "${testcvs} -Q co CVSROOT" ""
	    cd CVSROOT
	    echo "SystemAuth=no" >config
	    dotest pserver-2 "${testcvs} -q ci -m config-it" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	    cat >${CVSROOT_DIRNAME}/CVSROOT/passwd <<EOF
testme:q6WV9d2t848B2:$username
dontroot:q6WV9d2t848B2:root
anonymous::$username
$username:
willfail:   :whocares
EOF
	    dotest_fail pserver-3 "${testcvs} pserver" \
"error 0 Server configuration missing --allow-root in inetd.conf" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END AUTH REQUEST
EOF

	    dotest_fail pserver-3a "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"error 0 ${CVSROOT_DIRNAME}XXX: no such repository
I HATE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}XXX
testme
Ay::'d
END AUTH REQUEST
EOF

	    # Confirm that not sending a newline during auth cannot constitute
	    # a denial-of-service attack.  This assumes that PATH_MAX is less
	    # than 65536 bytes.  If PATH_MAX is larger than 65535 bytes, this
	    # test could hang indefinitely.
	    ${AWK} 'BEGIN { printf "0123456789abcdef" }' </dev/null >garbageseg
	    echo "BEGIN AUTH REQUEST" >garbageinput
	    i=0
	    while test $i -lt 64; do
	      cat <garbageseg >>garbageseg2
	      i=`expr $i + 1`
	    done
	    i=0
	    while test $i -lt 64; do
	      cat <garbageseg2 >>garbageinput
	      i=`expr $i + 1`
	    done
	    dotest_fail pserver-auth-no-dos \
"${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${PROG} \\[pserver aborted\\]: Maximum line length exceeded during authentication\." <garbageinput
	    unset i
	    rm garbageseg garbageseg2 garbageinput

	    # Sending the Root and noop before waiting for the
	    # "I LOVE YOU" is bogus, but hopefully we can get
	    # away with it.
	    dotest pserver-4 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
noop
EOF

	    # The "no such system user" error is occurring on at least one of
	    # our BSD 2.0.2 nightly test platforms.
	    dotest_fail pserver-4.2 \
"${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"error 0: root not allowed" \
"E Fatal error, aborting\.
error 0 root: no such system user" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
dontroot
Ay::'d
END AUTH REQUEST
EOF

	    dotest pserver-5 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
E Protocol error: Root says \"${TESTDIR}/1\" but pserver says \"${CVSROOT_DIRNAME}\"
error  " <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END AUTH REQUEST
Root ${TESTDIR}/1
noop
EOF

	    dotest pserver-5a "$testcvs --allow-root=$CVSROOT_DIRNAME pserver" \
"$DOTSTAR LOVE YOU
E init may not be run remotely
error  " <<EOF
BEGIN AUTH REQUEST
$CVSROOT_DIRNAME
testme
Ay::'d
END AUTH REQUEST
init $TESTDIR/2
EOF
	    dotest_fail pserver-5b "test -d $TESTDIR/2"

	    dotest_fail pserver-6 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"I HATE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d^b?hd
END AUTH REQUEST
EOF

	    dotest_fail pserver-7 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"I HATE YOU" <<EOF
BEGIN VERIFICATION REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d^b?hd
END VERIFICATION REQUEST
EOF

	    dotest pserver-8 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU" <<EOF
BEGIN VERIFICATION REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END VERIFICATION REQUEST
EOF

# Tests pserver-9 through pserver-13 are about empty passwords

            # Test empty password (both sides) for aliased user
	    dotest pserver-9 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
anonymous
A
END AUTH REQUEST
EOF

            # Test empty password (server side only) for aliased user
	    dotest pserver-10 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
anonymous
Aanythingwouldworkhereittrulydoesnotmatter
END AUTH REQUEST
EOF

            # Test empty (both sides) password for non-aliased user
	    dotest pserver-11 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
${username}
A
END AUTH REQUEST
EOF

            # Test empty (server side only) password for non-aliased user
	    dotest pserver-12 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
${username}
Anypasswordwouldworkwhynotthisonethen
END AUTH REQUEST
EOF

            # Test failure of whitespace password
	    dotest_fail pserver-13 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} HATE YOU" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
willfail
Amquiteunabletocomeupwithinterestingpasswordsanymore
END AUTH REQUEST
EOF

	    # The following tests are for read-only access

	    # Check that readers can only read, everyone else can write

	    cat >${CVSROOT_DIRNAME}/CVSROOT/readers <<EOF
anonymous
EOF

	    dotest pserver-14 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
anonymous
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    dotest pserver-16 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    dotest pserver-18 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
${username}
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    # Check that writers can write, everyone else can only read
	    # even if not listed in readers

	    cat >${CVSROOT_DIRNAME}/CVSROOT/writers <<EOF
testme
EOF

	    dotest pserver-20 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
anonymous
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    dotest pserver-22 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    dotest pserver-24 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
${username}
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    # Should work the same without readers

	    rm ${CVSROOT_DIRNAME}/CVSROOT/readers

	    dotest pserver-26 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
anonymous
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    dotest pserver-28 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
testme
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    dotest pserver-30 "${testcvs} --allow-root=${CVSROOT_DIRNAME} pserver" \
"${DOTSTAR} LOVE YOU
M Concurrent Versions System (CVS) .*
ok" <<EOF
BEGIN AUTH REQUEST
${CVSROOT_DIRNAME}
${username}
Ay::'d
END AUTH REQUEST
Root ${CVSROOT_DIRNAME}
version
EOF

	    # pserver used to try and print from the NULL pointer 
	    # in this error message in this case
	    dotest_fail pserver-bufinit "${testcvs} pserver" \
"${PROG} \[pserver aborted\]: bad auth protocol start: EOF" </dev/null

	    # Clean up.
	    echo "# comments only" >config
	    dotest pserver-cleanup-1 "${testcvs} -q ci -m config-it" \
"Checking in config;
${CVSROOT_DIRNAME}/CVSROOT/config,v  <--  config
new revision: 1\.[0-9]*; previous revision: 1\.[0-9]*
done
${PROG} commit: Rebuilding administrative file database"
	    cd ../..
	    rm -r 1
	    rm ${CVSROOT_DIRNAME}/CVSROOT/passwd ${CVSROOT_DIRNAME}/CVSROOT/writers
	  fi # skip the whole thing for local
	  ;;

	server)
	  # Some tests of the server (independent of the client).
	  if $remote; then
	    dotest server-1 "${testcvs} server" \
"E Protocol error: Root request missing
error  " <<EOF
Directory bogus
mumble/bar
update
EOF

	    # Could also test for relative pathnames here (so that crerepos-6a
	    # and crerepos-6b can use :fork:).
	    dotest server-2 "$testcvs server" \
"E init may not be run remotely
error  " <<EOF
Set OTHER=variable
Set MYENV=env-value
init ${TESTDIR}/crerepos
EOF
	    dotest_fail server-3 "test -d $TESTDIR/crerepos/CVSROOT"

	    dotest server-3a "$testcvs -d$TESTDIR/crerepos init"

	    # Now some tests of gzip-file-contents (used by jCVS).
	    ${AWK} 'BEGIN { \
printf "%c%c%c%c%c%c.6%c%c+I-.%c%c%c%c5%c;%c%c%c%c", \
31, 139, 8, 64, 5, 7, 64, 3, 225, 2, 64, 198, 185, 5, 64, 64, 64}' \
	      </dev/null | ${TR} '\100' '\000' >gzipped.dat
	    # Note that the CVS client sends "-b 1.1.1", and this
	    # test doesn't.  But the server also defaults to that.
	    cat <<EOF >session.dat
Root ${TESTDIR}/crerepos
UseUnchanged
gzip-file-contents 3
Argument -m
Argument msg
Argumentx 
Argument dir1
Argument tag1
Argument tag2
Directory .
${TESTDIR}/crerepos
Modified file1
u=rw,g=r,o=r
z25
EOF
	    cat gzipped.dat >>session.dat
	    echo import >>session.dat
	    dotest server-4 "${testcvs} server" \
"M N dir1/file1
M 
M No conflicts created by this import
M 
ok" <session.dat
	    dotest server-5 \
"${testcvs} -q -d ${TESTDIR}/crerepos co -p dir1/file1" "test"

	    # OK, here are some notify tests.
	    dotest server-6 "${testcvs} server" \
"Notified \./
${TESTDIR}/crerepos/dir1/file1
ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Notify file1
E	Fri May  7 13:21:09 1999 GMT	myhost	some-work-dir	EUC
noop
EOF
	    # Sending the second "noop" before waiting for the output
	    # from the first is bogus but hopefully we can get away
	    # with it.
	    dotest server-7 "${testcvs} server" \
"Notified \./
${TESTDIR}/crerepos/dir1/file1
ok
Notified \./
${TESTDIR}/crerepos/dir1/file1
ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Notify file1
E	Fri May  7 13:21:09 1999 GMT	myhost	some-work-dir	EUC
noop
Notify file1
E	The 57th day of Discord in the YOLD 3165	myhost	some-work-dir	EUC
noop
EOF

	    # OK, now test a few error conditions.
	    # FIXCVS: should give "error" and no "Notified", like server-9
	    dotest server-8 "${testcvs} server" \
"E ${PROG} server: invalid character in editor value
Notified \./
${TESTDIR}/crerepos/dir1/file1
ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Notify file1
E	Setting Orange, the 52th day of Discord in the YOLD 3165	myhost	some-work-dir	EUC
noop
EOF

	    dotest server-9 "${testcvs} server" \
"E Protocol error; misformed Notify request
error  " <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Notify file1
E	Setting Orange+57th day of Discord	myhost	some-work-dir	EUC
noop
EOF

	    # First demonstrate an interesting quirk in the protocol.
	    # The "watchers" request selects the files to operate based
	    # on files which exist in the working directory.  So if we
	    # don't send "Entry" or the like, it won't do anything.
	    # Wants to be documented in cvsclient.texi...
	    dotest server-10 "${testcvs} server" "ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
watchers
EOF
	    # See if "watchers" and "editors" display the right thing.
	    dotest server-11 "${testcvs} server" \
"M file1	${username}	tedit	tunedit	tcommit
ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Entry /file1/1.1////
watchers
EOF
	    dotest server-12 "${testcvs} server" \
"M file1	${username}	The 57th day of Discord in the YOLD 3165	myhost	some-work-dir
ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Entry /file1/1.1////
editors
EOF

	    # Now do an unedit.
	    dotest server-13 "${testcvs} server" \
"Notified \./
${TESTDIR}/crerepos/dir1/file1
ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
Notify file1
U	7 May 1999 15:00 GMT	myhost	some-work-dir	EUC
noop
EOF

	    # Now try "watchers" and "editors" again.
	    dotest server-14 "${testcvs} server" "ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
watchers
EOF
	    dotest server-15 "${testcvs} server" "ok" <<EOF
Root ${TESTDIR}/crerepos
Directory .
${TESTDIR}/crerepos/dir1
editors
EOF

	    # Test that the global `-l' option is ignored nonfatally.
	    dotest server-16 "${testcvs} server" \
"E cvs server: WARNING: global \`-l' option ignored\.
ok" <<EOF
Global_option -l
noop
EOF

	    # There used to be some exploits based on malformed Entry requests
	    dotest server-17 "$testcvs server" \
"E protocol error: Malformed Entry
error  " <<EOF
Root $TESTDIR/crerepos
Directory .
$TESTDIR/crerepos/dir1
Entry X/file1/1.1////
noop
EOF

	    dotest server-18 "$testcvs server" \
"E protocol error: Malformed Entry
error  " <<EOF
Root $TESTDIR/crerepos
Directory .
$TESTDIR/crerepos/dir1
Entry /CC/CC/CC
noop
EOF

	    if $keep; then
	      echo Keeping ${TESTDIR} and exiting due to --keep
	      exit 0
	    fi

	    rm -rf ${TESTDIR}/crerepos
	    rm gzipped.dat session.dat
	  fi # skip the whole thing for local
	  ;;

	server2)
	  # More server tests, in particular testing that various
	  # possible security holes are plugged.
	  if $remote; then
	    dotest server2-1 "${testcvs} server" \
"E protocol error: directory '${CVSROOT_DIRNAME}/\.\./dir1' not within root '${CVSROOT_DIRNAME}'
error  " <<EOF
Root ${CVSROOT_DIRNAME}
Directory .
${CVSROOT_DIRNAME}/../dir1
noop
EOF

	    dotest server2-2 "${testcvs} server" \
"E protocol error: directory '${CVSROOT_DIRNAME}dir1' not within root '${CVSROOT_DIRNAME}'
error  " <<EOF
Root ${CVSROOT_DIRNAME}
Directory .
${CVSROOT_DIRNAME}dir1
noop
EOF

	    dotest server2-3 "${testcvs} server" \
"E protocol error: directory '${TESTDIR}' not within root '${CVSROOT_DIRNAME}'
error  " <<EOF
Root ${CVSROOT_DIRNAME}
Directory .
${TESTDIR}
noop
EOF

	    # OK, now a few tests for the rule that one cannot pass a
	    # filename containing a slash to Modified, Is-modified,
	    # Notify, Questionable, or Unchanged.  For completeness
	    # we'd try them all.  For lazyness/conciseness we don't.
	    dotest server2-4 "${testcvs} server" \
"E protocol error: directory 'foo/bar' not within current directory
error  " <<EOF
Root ${CVSROOT_DIRNAME}
Directory .
${CVSROOT_DIRNAME}
Unchanged foo/bar
noop
EOF
	    dotest server2-5 \
"$testcvs --allow-root=$CVSROOT_DIRNAME.bad server" \
"E Bad root $CVSROOT_DIRNAME
error  " <<EOF
Root $CVSROOT_DIRNAME
noop
EOF
	    dotest server2-6 \
"$testcvs --allow-root=$CVSROOT_DIRNAME server" \
"ok" <<EOF
Root $CVSROOT_DIRNAME
noop
EOF
	  fi
	  ;;



	server3)
	  # Test that various checks on the Root request generate the correct
	  # error messages.
	  if $remote; then
	    # As a control, a valid request.
	    dotest server3-1 "$testcvs server" 'ok' <<EOF
Root $CVSROOT_DIRNAME
Directory .
$CVSROOT_DIRNAME
Unchanged foo
noop
EOF

	    dotest server3-2 "$testcvs server" \
"E Root somewhere/over/the/rainbow must be an absolute pathname
error  " <<EOF
Root somewhere/over/the/rainbow
noop
EOF

	    dotest server3-3 "$testcvs server" \
"E Protocol error: Duplicate Root request, for $CVSROOT_DIRNAME
error  " <<EOF
Root $CVSROOT_DIRNAME
Root $CVSROOT_DIRNAME
noop
EOF

	    dotest server3-4 "$testcvs server" \
"E Protocol error: Duplicate Root request, for $CVSROOT_DIRNAME
error  " <<EOF
Root $CVSROOT_DIRNAME
Root $CVSROOT_DIRNAME
Directory .
$CVSROOT_DIRNAME
Unchanged foo
noop
EOF

	    # These cascading errors seem odd, but the client should have hung
	    # up after the first.
	    dotest server3-5 "$testcvs server" \
"E Root somewhere/over/the/rainbow must be an absolute pathname
error  
E Protocol error: Root request missing
error  " <<EOF
Root somewhere/over/the/rainbow
Directory .
somewhere/over/the/rainbow
Unchanged foo
noop
EOF
	  fi
	  ;;



	client)
	  # Some tests of the client (independent of the server).
	  if $remote; then
	    cat >${TESTDIR}/serveme <<EOF
#!${TESTSHELL}
# This is admittedly a bit cheezy, in the sense that we make lots
# of assumptions about what the client is going to send us.
# We don't mention Repository, because current clients don't require it.
# Sending these at our own pace, rather than waiting for the client to
# make the requests, is bogus, but hopefully we can get away with it.
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "M special message"
echo "Created first-dir/"
echo "${CVSROOT_DIRNAME}/first-dir/file1"
echo "/file1/1.1///"
echo "u=rw,g=rw,o=rw"
echo "4"
echo "xyz"
echo "ok"
cat >/dev/null
EOF
	    # Cygwin.  Pthffffffffft!
	    if test -n "$remotehost"; then
	      $CVS_RSH $remotehost "chmod +x ${TESTDIR}/serveme"
	    else
	      chmod +x ${TESTDIR}/serveme
	    fi
	    save_CVS_SERVER=$CVS_SERVER
	    CVS_SERVER=${TESTDIR}/serveme; export CVS_SERVER
	    mkdir 1; cd 1
	    dotest_fail client-1 "${testcvs} -q co first-dir" \
"${PROG} \[checkout aborted\]: This server does not support the global -q option${DOTSTAR}"
	    dotest client-2 "${testcvs} co first-dir" "special message"

	    cat >${TESTDIR}/serveme <<EOF
#!${TESTSHELL}
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "M merge-it"
echo "Copy-file ./"
echo "${CVSROOT_DIRNAME}/first-dir/file1"
echo "${TESTDIR}/bogus/.#file1.1.1"
echo "Merged ./"
echo "${CVSROOT_DIRNAME}/first-dir/file1"
echo "/file1/1.2///"
echo "u=rw,g=rw,o=rw"
echo "4"
echo "abd"
echo "ok"
cat >/dev/null
EOF
	    cd first-dir
	    mkdir ${TESTDIR}/bogus
	    # The ${DOTSTAR} is to match a potential "broken pipe" if the
	    # client exits before the server script sends everything
	    dotest_fail client-3 "${testcvs} update" "merge-it
${PROG} \[update aborted\]: protocol error: Copy-file tried to specify director${DOTSTAR}"
	    cat >${TESTDIR}/serveme <<EOF
#!${TESTSHELL}
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "M merge-it"
echo "Copy-file ./"
echo "${CVSROOT_DIRNAME}/first-dir/file1"
echo ".#file1.1.1"
echo "Merged ./"
echo "${CVSROOT_DIRNAME}/first-dir/file1"
echo "/file1/1.2///"
echo "u=rw,g=rw,o=rw"
echo "4"
echo "abc"
echo "ok"
cat >/dev/null
EOF
	    dotest client-4 "${testcvs} update" "merge-it"
	    dotest client-5 "cat .#file1.1.1" "xyz"
	    dotest client-6 "cat CVS/Entries" "/file1/1.2/[A-Za-z0-9 :]*//
D"
	    dotest client-7 "cat file1" "abc"

	    cat >${TESTDIR}/serveme <<EOF
#!${TESTSHELL}
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "M OK, whatever"
echo "ok"
cat >${TESTDIR}/client.tmp
EOF
	    chmod u=rw,go= file1
	    # By specifying the time zone in local time, we don't
	    # know exactly how that will translate to GMT.
	    dotest client-8 "${testcvs} update -D 99-10-04" "OK, whatever"
	    # String 2 below is Cygwin again - ptoooey.
	    dotest client-9 "cat ${TESTDIR}/client.tmp" \
"Root ${CVSROOT_DIRNAME}
Valid-responses [-a-zA-Z ]*
valid-requests
Argument -D
Argument [34] Oct 1999 [0-9][0-9]:00:00 -0000
Argument --
Directory \.
${CVSROOT_DIRNAME}/first-dir
Entry /file1/1\.2///
Modified file1
u=rw,g=,o=
4
abc
update" \
"Root ${CVSROOT_DIRNAME}
Valid-responses [-a-zA-Z ]*
valid-requests
Argument -D
Argument [34] Oct 1999 [0-9][0-9]:00:00 -0000
Argument --
Directory \.
${CVSROOT_DIRNAME}/first-dir
Entry /file1/1\.2///
Modified file1
u=rw,g=r,o=r
4
abc
update"

	    # The following test tests what was a potential client update in
	    # CVS versions 1.11.14 and CVS versions 1.12.6 and earlier.  This
	    # exploit would allow a trojan server to create arbitrary files,
	    # anywhere the user had write permissions, even outside of the
	    # user's sandbox.
	    cat >$HOME/.bashrc <<EOF
#!$TESTSHELL
# This is where login scripts would usually be
# stored.
EOF
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Rcs-diff $HOME/"
echo "$HOME/.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "20"
echo "a1 1"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    
	    # If I don't run the following sleep between the above cat and
	    # the following calls to dotest, sometimes the serveme file isn't
	    # completely written yet by the time CVS tries to execute it,
	    # causing the shell to intermittantly report syntax errors (usually
	    # early EOF).  There's probably a new race condition here, but this
	    # works.
	    #
	    # Incidentally, I can reproduce this behavior with Linux 2.4.20 and
	    # Bash 2.05 or Bash 2.05b.
	    sleep 1
	    dotest_fail client-10 "$testcvs update" \
"$PROG update: Server attempted to update a file via an invalid pathname:
$PROG \[update aborted\]: \`$HOME/.bashrc'\."

	    # A second try at a client exploit.  This one never actually
	    # failed in the past, but I thought it wouldn't hurt to add a test.
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Rcs-diff ./"
echo "$HOME/.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "20"
echo "a1 1"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-11 "$testcvs update" \
"$PROG \[update aborted\]: patch original file \./\.bashrc does not exist"

	    # A third try at a client exploit.  This one did used to fail like
	    # client-10.
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Rcs-diff ../../home/"
echo "../../.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "20"
echo "a1 1"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-12 "$testcvs update" \
"$PROG update: Server attempted to update a file via an invalid pathname:
$PROG \[update aborted\]: \`\.\./\.\./home/.bashrc'\."

	    # Try the same exploit using the Created response.
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Created $HOME/"
echo "$HOME/.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "26"
echo "#! /bin/sh"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-13 "$testcvs update" \
"$PROG update: Server attempted to update a file via an invalid pathname:
$PROG \[update aborted\]: \`$HOME/.bashrc'\."

	    # Now try using the Update-existing response
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Update-existing ../../home/"
echo "../../home/.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "26"
echo "#! /bin/sh"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-14 "$testcvs update" \
"$PROG update: Server attempted to update a file via an invalid pathname:
$PROG \[update aborted\]: \`\.\./\.\./home/.bashrc'\."

	    # Try the same exploit using the Merged response.
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Merged $HOME/"
echo "$HOME/.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "26"
echo "#! /bin/sh"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-15 "$testcvs update" \
"$PROG update: Server attempted to update a file via an invalid pathname:
$PROG \[update aborted\]: \`$HOME/.bashrc'\."

	    # Now try using the Updated response
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Updated ../../home/"
echo "../../home/.bashrc"
echo "/.bashrc/73.50///"
echo "u=rw,g=rw,o=rw"
echo "26"
echo "#! /bin/sh"
echo "echo 'gotcha!'"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-16 "$testcvs update" \
"$PROG update: Server attempted to update a file via an invalid pathname:
$PROG \[update aborted\]: \`\.\./\.\./home/.bashrc'\."

	    # Try the same exploit using the Copy-file response.
	    # As far as I know, Copy-file was never exploitable either.
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "ok"
echo "Created ."
echo "./innocuous"
echo "/innocuous/73.50///"
echo "u=rw,g=rw,o=rw"
echo "26"
echo "#! /bin/sh"
echo "echo 'gotcha!'"
echo "Copy-file ."
echo "./innocuous"
echo "$HOME/innocuous"
echo "ok"
cat >/dev/null
EOF
	    sleep 1
	    dotest_fail client-18 "$testcvs update" \
"$PROG \[update aborted\]: protocol error: Copy-file tried to specify directory"

	    # And verify that none of the exploits was successful.
	    dotest client-19 "cat $HOME/.bashrc" \
"#!$TESTSHELL
# This is where login scripts would usually be
# stored\."

	    if $keep; then
	      echo Keeping ${TESTDIR} and exiting due to --keep
	      exit 0
	    fi

	    cd ../..
	    rm -r 1
	    rmdir ${TESTDIR}/bogus
	    rm $TESTDIR/serveme $HOME/.bashrc
	    CVS_SERVER=${save_CVS_SERVER}; export CVS_SERVER
	  fi # skip the whole thing for local
	  ;;



	client2)
	  # Test how the client handles error messages from the server.
	  if $remote; then
	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
# This is just as cheesy as the "client" tests made it out to be.
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "E Root somewhere/over/the/rainbow must be an absolute pathname"
echo "error  "
echo "E Protocol error: Root request missing"
echo "error  "
cat >/dev/null
EOF
	    # Cygwin.  Pthffffffffft!
	    if test -n "$remotehost"; then
	      $CVS_RSH $remotehost "chmod +x $TESTDIR/serveme"
	    else
	      chmod +x $TESTDIR/serveme
	    fi
	    save_CVS_SERVER=$CVS_SERVER
	    CVS_SERVER=$TESTDIR/serveme; export CVS_SERVER
	    mkdir client2; cd client2
	    dotest_fail client2-1 "$testcvs co first-dir" \
"Root somewhere/over/the/rainbow must be an absolute pathname"

	    cat >$TESTDIR/serveme <<EOF
#!$TESTSHELL
# This is just as cheesy as the "client" tests made it out to be.
echo "Valid-requests Root Valid-responses valid-requests Directory Entry Modified Unchanged Argument Argumentx ci co update"
echo "E Root somewhere/over/the/rainbow must be an absolute pathname"
echo
echo "error  "
echo "E Protocol error: Root request missing"
echo "error  "
cat >/dev/null
EOF
	    # Cygwin.  Pthffffffffft!
	    if test -n "$remotehost"; then
	      $CVS_RSH $remotehost "chmod +x $TESTDIR/serveme"
	    else
	      chmod +x $TESTDIR/serveme
	    fi
	    dotest_fail client2-2 "$testcvs co first-dir" \
"Root somewhere/over/the/rainbow must be an absolute pathname
$PROG checkout: warning: unrecognized response \`' from cvs server"

	    if $keep; then
	      echo Keeping $TESTDIR and exiting due to --keep
	      exit 0
	    fi

	    cd ..
	    rm -r client2
	    rm $TESTDIR/serveme
	    CVS_SERVER=$save_CVS_SERVER; export CVS_SERVER
	  fi # skip the whole thing for local
	  ;;



	dottedroot)
	  # Check that a CVSROOT with a "." in the name will work.
	  CVSROOT_save=${CVSROOT}
	  CVSROOT_DIRNAME_save=${CVSROOT_DIRNAME}
	  CVSROOT_DIRNAME=${TESTDIR}/cvs.root
	  CVSROOT=`newroot ${CVSROOT_DIRNAME}`

	  dotest dottedroot-init-1 "$testcvs -d$CVSROOT_DIRNAME init"
	  mkdir dir1
	  mkdir dir1/dir2
	  echo version1 >dir1/dir2/file1
	  cd dir1
	  dotest dottedroot-1 "${testcvs} import -m '' module1 AUTHOR INITIAL" \
"${PROG} [a-z]*: Importing ${CVSROOT_DIRNAME}/module1/dir2
N module1/dir2/file1

No conflicts created by this import"
	  cd ..

	  # This is the test that used to cause an assertion failure
	  # in recurse.c:do_recursion().
	  dotest dottedroot-2 "${testcvs} co -rINITIAL module1" \
"${PROG} [a-z]*: Updating module1
${PROG} [a-z]*: Updating module1/dir2
U module1/dir2/file1"

	  # This also triggered the assertion failure above prior to 1.11.23.
	  dotest dottedroot-3 \
"$testcvs -q co -prINITIAL module1/./dir2/file1" \
'version1'

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi

	  rm -rf ${CVSROOT_DIRNAME}
	  rm -r dir1 module1
	  CVSROOT_DIRNAME=${CVSROOT_DIRNAME_save}
	  CVSROOT=${CVSROOT_save}
 	  ;;
 
	fork)
	  # Test that the server defaults to the correct executable in :fork:
	  # mode.  See the note in the TODO at the end of this file about this.
	  #
	  # This test and client should be left after all other references to
	  # CVS_SERVER are removed from this script.
	  #
	  # The client series of tests already tests that CVS_SERVER is
	  # working, but that test might be better here.
	  if $remote; then
	    mkdir fork; cd fork
	    save_CVS_SERVER=$CVS_SERVER
	    unset CVS_SERVER
	    # So looking through $PATH for cvs won't work...
	    echo "echo junk" >cvs
	    chmod a+x cvs
	    save_PATH=$PATH; PATH=.:$PATH
	    dotest fork-1 "$testcvs -d:fork:$CVSROOT_DIRNAME version" \
'Client: \(.*\)
Server: \1'
	    CVS_SERVER=${save_CVS_SERVER}; export CVS_SERVER
	    unset save_CVS_SERVER
	    PATH=$save_PATH; unset save_PATH
	    cd ..

	    if $keep; then
	      echo Keeping ${TESTDIR} and exiting due to --keep
	      exit 0
	    fi
	  fi
	  ;;

	commit-add-missing)
	  # Make sure that a commit fails when a `cvs add'ed file has
	  # been removed from the working directory.

	  mkdir 1; cd 1
	  module=c-a-m
	  echo > unused-file
	  dotest commit-add-missing-1 \
	    "$testcvs -Q import -m. $module X Y" ''

	  file=F
	  # Check it out and tag it.
	  dotest commit-add-missing-2 "$testcvs -Q co $module" ''
	  cd $module
	  dotest commit-add-missing-3 "$testcvs -Q tag -b B" ''
	  echo v1 > $file
	  dotest commit-add-missing-4 "$testcvs -Q add $file" ''
	  rm -f $file
	  dotest_fail commit-add-missing-5 "$testcvs -Q ci -m. $file" \
"${PROG} commit: Up-to-date check failed for .$file'
${PROG} \[commit aborted\]: correct above errors first!"

	  cd ../..
	  rm -rf 1
	  rm -rf ${CVSROOT_DIRNAME}/$module
	  ;;

	add-restricted)
	  # Verify that `sdir/CVS' may not be explicitly added.
	  mkdir add-restricted; cd add-restricted

	  mkdir import; cd import
	  : > junk
	  dotest add-restricted-init-1 \
"$testcvs -Q import -m. add-restricted X Y"
	  cd ..

	  dotest add-restricted-init-2 "$testcvs -Q co add-restricted"
	  cd add-restricted

	  # errmsg2-3 tests the specific message here.
	  dotest_fail add-restricted-1 "$testcvs -Q add CVS"

	  mkdir sdir
	  dotest add-restricted-2 "$testcvs -Q add sdir"
	  dotest_fail add-restricted-3 "$testcvs add sdir/CVS" \
"$PROG add: cannot add special file \`sdir/CVS'; skipping"

	  if $keep; then
	    echo Keeping $TESTDIR and exiting due to --keep
	    exit 0
	  fi
	  cd ../..
	  rm -rf add-restricted $CVSROOT_DIRNAME/add-restricted
	;;



	commit-d)
	  # Check that top-level commits work when CVS/Root
	  # is overridden by cvs -d.

	  mkdir -p 1/subdir; cd 1
	  touch file1 subdir/file2
	  dotest commit-d-1 "$testcvs -Q import -m. c-d-c X Y" ""
	  dotest commit-d-2 "$testcvs -Q co c-d-c" ""
	  cd c-d-c
	  echo change >>file1; echo another change >>subdir/file2
	  # Changing working root, then override with -d
	  echo nosuchhost:/cvs > CVS/Root
	  dotest commit-d-3 "$testcvs -Q -d $CVSROOT commit -m." \
"Checking in file1;
${CVSROOT_DIRNAME}/c-d-c/file1,v  <--  file1
new revision: 1.2; previous revision: 1.1
done
Checking in subdir/file2;
${CVSROOT_DIRNAME}/c-d-c/subdir/file2,v  <--  file2
new revision: 1.2; previous revision: 1.1
done"
	  cd ../..
	  rm -rf 1 cvsroot/c-d-c
	  ;;

	*)
	   echo $what is not the name of a test -- ignored
	   ;;
	esac

    # Sanity check sanity.sh.  :)
    #
    # Test our exit directory so that tests that exit in an incorrect directory
    # are noticed during single test runs.
    #
    # FIXME?
    # Sparc Solaris 9 is dereferencing paths here as if /bin/pwd were
    # called when /tmp is a symlink.  This might be a new problem with this
    # test, but since this was recently tested I think it more likely to be
    # A Solaris issue.
    if test "x$TESTDIR" != "x`pwd`"; then
	    fail "cleanup: PWD != TESTDIR (\``pwd`' != \`$TESTDIR')"
    fi

    # Reset val-tags to a pristine state.
    rm -f $CVSROOT_DIRNAME/CVSROOT/val-tags

    verify_tmp_empty "post $what"

done # The big loop

# Set up summary data for output.
skippedoutput=
warningsoutput=
extendedinfo=
if test $skipped -ne 0; then
  skippedoutput="$skipped test group"
  if test $skipped -ne 1; then
    skippedoutput="${skippedoutput}s"
  fi
  skippedoutput="$skippedoutput skipped"
fi
if test $warnings -ne 0; then
  warningsoutput="$warnings test"
  if test $warnings -ne 1; then
    warningsoutput="${warningsoutput}s"
  fi
  warningsoutput="$warningsoutput passed with warnings"
fi
if test -n "$skippedoutput" || test -n "$warningsoutput"; then
  extendedinfo=" ("
  if test -n "$skippedoutput"; then
    extendedinfo="$extendedinfo$skippedoutput"
  fi
  if test -n "$skippedoutput" && test -n "$warningsoutput"; then
    extendedinfo="$extendedinfo and "
  fi
  if test -n "$warningsoutput"; then
    extendedinfo="$extendedinfo$warningsoutput"
  fi
  extendedinfo="$extendedinfo)"
fi

echo "OK, all $passed tests passed$extendedinfo."

# TODO:
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
# * Test the contents of adm files other than Root and Repository.
#   Entries seems the next most important thing.
# * Test the following compatibility issues:
#   - The filler fields in "D" entries in CVS/Entries get preserved
#     (per cvs.texinfo).
#   - Unrecognized entry types in CVS/Entries get ignored (looks like
#     this needs to be documented in cvs.texinfo, but is not)
#   - Test that unrecognized files in CVS directories (e.g. CVS/Foobar)
#     are ignored (per cvs.texinfo).
#   - Test 'cvs history' with symlinks in the path to the working directory.
#   - Remove most of the CVS_SERVER stuff after a reasonable amount of time.
#     The "fork" & "client" series of tests should be left.  4/2/00, CVS
#     1.11.0.1 was altered so that it would default to program_name (set from
#     argv[0]) rather than "cvs", but I'd like this script to work on legacy
#     versions of CVS for awhile.
#   - Testsuite doesn't work with usernames over eight characters in length.
#     Fix it.
# End of TODO list.

# Exit if keep set
if $keep; then
  echo "Keeping ${TESTDIR} and exiting due to -k (keep) option."
  exit 0
fi

# Remove the test directory, but first change out of it.
cd `dirname ${TESTDIR}`
rm -rf ${TESTDIR}

# end of sanity.sh
