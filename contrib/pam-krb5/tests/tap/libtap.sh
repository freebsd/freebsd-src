# Shell function library for test cases.
#
# Note that while many of the functions in this library could benefit from
# using "local" to avoid possibly hammering global variables, Solaris /bin/sh
# doesn't support local and this library aspires to be portable to Solaris
# Bourne shell.  Instead, all private variables are prefixed with "tap_".
#
# This file provides a TAP-compatible shell function library useful for
# writing test cases.  It is part of C TAP Harness, which can be found at
# <https://www.eyrie.org/~eagle/software/c-tap-harness/>.
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2009-2012, 2016 Russ Allbery <eagle@eyrie.org>
# Copyright 2006-2008, 2013
#     The Board of Trustees of the Leland Stanford Junior University
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
# SPDX-License-Identifier: MIT

# Print out the number of test cases we expect to run.
plan () {
    count=1
    planned="$1"
    failed=0
    echo "1..$1"
    trap finish 0
}

# Prepare for lazy planning.
plan_lazy () {
    count=1
    planned=0
    failed=0
    trap finish 0
}

# Report the test status on exit.
finish () {
    tap_highest=`expr "$count" - 1`
    if [ "$planned" = 0 ] ; then
        echo "1..$tap_highest"
        planned="$tap_highest"
    fi
    tap_looks='# Looks like you'
    if [ "$planned" -gt 0 ] ; then
        if [ "$planned" -gt "$tap_highest" ] ; then
            if [ "$planned" -gt 1 ] ; then
                echo "$tap_looks planned $planned tests but only ran" \
                    "$tap_highest"
            else
                echo "$tap_looks planned $planned test but only ran" \
                    "$tap_highest"
            fi
        elif [ "$planned" -lt "$tap_highest" ] ; then
            tap_extra=`expr "$tap_highest" - "$planned"`
            if [ "$planned" -gt 1 ] ; then
                echo "$tap_looks planned $planned tests but ran" \
                    "$tap_extra extra"
            else
                echo "$tap_looks planned $planned test but ran" \
                    "$tap_extra extra"
            fi
        elif [ "$failed" -gt 0 ] ; then
            if [ "$failed" -gt 1 ] ; then
                echo "$tap_looks failed $failed tests of $planned"
            else
                echo "$tap_looks failed $failed test of $planned"
            fi
        elif [ "$planned" -gt 1 ] ; then
            echo "# All $planned tests successful or skipped"
        else
            echo "# $planned test successful or skipped"
        fi
    fi
}

# Skip the entire test suite.  Should be run instead of plan.
skip_all () {
    tap_desc="$1"
    if [ -n "$tap_desc" ] ; then
        echo "1..0 # skip $tap_desc"
    else
        echo "1..0 # skip"
    fi
    exit 0
}

# ok takes a test description and a command to run and prints success if that
# command is successful, false otherwise.  The count starts at 1 and is
# updated each time ok is printed.
ok () {
    tap_desc="$1"
    if [ -n "$tap_desc" ] ; then
        tap_desc=" - $tap_desc"
    fi
    shift
    if "$@" ; then
        echo ok "$count$tap_desc"
    else
        echo not ok "$count$tap_desc"
        failed=`expr $failed + 1`
    fi
    count=`expr $count + 1`
}

# Skip the next test.  Takes the reason why the test is skipped.
skip () {
    echo "ok $count # skip $*"
    count=`expr $count + 1`
}

# Report the same status on a whole set of tests.  Takes the count of tests,
# the description, and then the command to run to determine the status.
ok_block () {
    tap_i=$count
    tap_end=`expr $count + $1`
    shift
    while [ "$tap_i" -lt "$tap_end" ] ; do
        ok "$@"
        tap_i=`expr $tap_i + 1`
    done
}

# Skip a whole set of tests.  Takes the count and then the reason for skipping
# the test.
skip_block () {
    tap_i=$count
    tap_end=`expr $count + $1`
    shift
    while [ "$tap_i" -lt "$tap_end" ] ; do
        skip "$@"
        tap_i=`expr $tap_i + 1`
    done
}

# Portable variant of printf '%s\n' "$*".  In the majority of cases, this
# function is slower than printf, because the latter is often implemented
# as a builtin command.  The value of the variable IFS is ignored.
#
# This macro must not be called via backticks inside double quotes, since this
# will result in bizarre escaping behavior and lots of extra backslashes on
# Solaris.
puts () {
    cat << EOH
$@
EOH
}

# Run a program expected to succeed, and print ok if it does and produces the
# correct output.  Takes the description, expected exit status, the expected
# output, the command to run, and then any arguments for that command.
# Standard output and standard error are combined when analyzing the output of
# the command.
#
# If the command may contain system-specific error messages in its output,
# add strip_colon_error before the command to post-process its output.
ok_program () {
    tap_desc="$1"
    shift
    tap_w_status="$1"
    shift
    tap_w_output="$1"
    shift
    tap_output=`"$@" 2>&1`
    tap_status=$?
    if [ $tap_status = $tap_w_status ] \
        && [ x"$tap_output" = x"$tap_w_output" ] ; then
        ok "$tap_desc" true
    else
        echo "#  saw: ($tap_status) $tap_output"
        echo "#  not: ($tap_w_status) $tap_w_output"
        ok "$tap_desc" false
    fi
}

# Strip a colon and everything after it off the output of a command, as long
# as that colon comes after at least one whitespace character.  (This is done
# to avoid stripping the name of the program from the start of an error
# message.)  This is used to remove system-specific error messages (coming
# from strerror, for example).
strip_colon_error() {
    tap_output=`"$@" 2>&1`
    tap_status=$?
    tap_output=`puts "$tap_output" | sed 's/^\([^ ]* [^:]*\):.*/\1/'`
    puts "$tap_output"
    return $tap_status
}

# Bail out with an error message.
bail () {
    echo 'Bail out!' "$@"
    exit 255
}

# Output a diagnostic on standard error, preceded by the required # mark.
diag () {
    echo '#' "$@"
}

# Search for the given file first in $C_TAP_BUILD and then in $C_TAP_SOURCE
# and echo the path where the file was found, or the empty string if the file
# wasn't found.
#
# This macro uses puts, so don't run it using backticks inside double quotes
# or bizarre quoting behavior will happen with Solaris sh.
test_file_path () {
    if [ -n "$C_TAP_BUILD" ] && [ -f "$C_TAP_BUILD/$1" ] ; then
        puts "$C_TAP_BUILD/$1"
    elif [ -n "$C_TAP_SOURCE" ] && [ -f "$C_TAP_SOURCE/$1" ] ; then
        puts "$C_TAP_SOURCE/$1"
    else
        echo ''
    fi
}

# Create $C_TAP_BUILD/tmp for use by tests for storing temporary files and
# return the path (via standard output).
#
# This macro uses puts, so don't run it using backticks inside double quotes
# or bizarre quoting behavior will happen with Solaris sh.
test_tmpdir () {
    if [ -z "$C_TAP_BUILD" ] ; then
        tap_tmpdir="./tmp"
    else
        tap_tmpdir="$C_TAP_BUILD"/tmp
    fi
    if [ ! -d "$tap_tmpdir" ] ; then
        mkdir "$tap_tmpdir" || bail "Error creating $tap_tmpdir"
    fi
    puts "$tap_tmpdir"
}
