#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Klara, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# sysexits(3)
: ${EX_USAGE:=64}
: ${EX_UNAVAILABLE:=69}
: ${EX_CANTCREAT:=73}
: ${EX_TEMPFAIL:=75}

atf_test_case badargs
badargs_body()
{
	atf_check -s exit:${EX_USAGE} -e not-empty lockf
	atf_check -s exit:${EX_USAGE} -e not-empty lockf "testlock"
}

atf_test_case basic
basic_body()
{
	# Something innocent so that it does eventually go away without our
	# intervention.
	lockf "testlock" sleep 10 &
	lpid=$!

	# Make sure that the lock exists...
	atf_check test -e "testlock"

	# Attempt both verbose and silent re-lock
	atf_check -s exit:${EX_TEMPFAIL} -e not-empty \
	    lockf -t 0 "testlock" sleep 0
	atf_check -s exit:${EX_TEMPFAIL} -e empty \
	    lockf -t 0 -s "testlock" sleep 0

	# Make sure it cleans up after the initial sleep 10 is over.
	wait "$lpid"
	atf_check test ! -e "testlock"
}

atf_test_case fdlock
fdlock_body()
{
	# First, make sure we don't get a false positive -- existing uses with
	# numeric filenames shouldn't switch to being fdlocks automatically.
	atf_check lockf -k "9" sleep 0
	atf_check test -e "9"
	rm "9"

	subexit_lockfail=1
	subexit_created=2
	subexit_lockok=3
	subexit_concurrent=4
	(
		lockf -s -t 0 9
		if [ $? -ne 0 ]; then
			exit "$subexit_lockfail"
		fi

		if [ -e "9" ]; then
			exit "$subexit_created"
		fi
	) 9> "testlock1"
	rc=$?

	atf_check test "$rc" -eq 0

	sub_delay=5

	# But is it actually locking?  Child 1 will acquire the lock and then
	# signal that it's ok for the second child to try.  The second child
	# will try to acquire the lock and fail immediately, signal that it
	# tried, then try again with an indefinite timeout.  On that one, we'll
	# just check how long we ended up waiting -- it should be at least
	# $sub_delay.
	(
		lockf -s -t 0 /dev/fd/9
		if [ $? -ne 0 ]; then
			exit "$subexit_lockfail"
		fi

		# Signal
		touch ".lock_acquired"

		while [ ! -e ".lock_attempted" ]; do
			sleep 0.5
		done

		sleep "$sub_delay"

		if [ -e ".lock_acquired_again" ]; then
			exit "$subexit_concurrent"
		fi
	) 9> "testlock2" &
	lpid1=$!

	(
		while [ ! -e ".lock_acquired" ]; do
			sleep 0.5
		done

		# Got the signal, try
		lockf -s -t 0 9
		if [ $? -ne "${EX_TEMPFAIL}" ]; then
			exit "$subexit_lockok"
		fi

		touch ".lock_attempted"
		start=$(date +"%s")
		lockf -s 9
		touch ".lock_acquired_again"
		now=$(date +"%s")
		elapsed=$((now - start))

		if [ "$elapsed" -lt "$sub_delay" ]; then
			exit "$subexit_concurrent"
		fi
	) 9> "testlock2" &
	lpid2=$!

	wait "$lpid1"
	status1=$?

	wait "$lpid2"
	status2=$?

	atf_check test "$status1" -eq 0
	atf_check test "$status2" -eq 0
}

atf_test_case keep
keep_body()
{
	lockf -k "testlock" sleep 10 &
	lpid=$!

	# Make sure that the lock exists now...
	while ! test -e "testlock"; do
		sleep 0.5
	done

	kill "$lpid"
	wait "$lpid"

	# And it still exits after the lock has been relinquished.
	atf_check test -e "testlock"
}

atf_test_case needfile
needfile_body()
{
	# Hopefully the clock doesn't jump.
	start=$(date +"%s")

	# Should fail if the lockfile does not yet exist.
	atf_check -s exit:"${EX_UNAVAILABLE}" lockf -sn "testlock" sleep 30

	# It's hard to guess how quickly we should have finished that; one would
	# hope that it exits fast, but to be safe we specified a sleep 30 under
	# lock so that we have a good margin below that duration that we can
	# safely test to make sure we didn't actually execute the program, more
	# or less.
	now=$(date +"%s")
	tpass=$((now - start))
	atf_check test "$tpass" -lt 10
}

atf_test_case timeout
timeout_body()
{
	lockf "testlock" sleep 30 &
	lpid=$!

	while ! test -e "testlock"; do
		sleep 0.5
	done

	start=$(date +"%s")
	timeout=2
	atf_check -s exit:${EX_TEMPFAIL} lockf -st "$timeout" "testlock" sleep 0

	# We should have taken no less than our timeout, at least.
	now=$(date +"%s")
	tpass=$((now - start))
	atf_check test "$tpass" -ge "$timeout"

	kill "$lpid"
	wait "$lpid" || true
}

atf_test_case wrlock
wrlock_head()
{
	atf_set "require.user" "unprivileged"
}
wrlock_body()
{
	touch "testlock"
	chmod -w "testlock"

	# Demonstrate that we can lock the file normally, but -w fails if we
	# can't write.
	atf_check lockf -kt 0 "testlock" sleep 0
	atf_check -s exit:${EX_CANTCREAT} -e not-empty \
	    lockf -wt 0 "testlock" sleep 0
}

atf_init_test_cases()
{
	atf_add_test_case badargs
	atf_add_test_case basic
	atf_add_test_case fdlock
	atf_add_test_case keep
	atf_add_test_case needfile
	atf_add_test_case timeout
	atf_add_test_case wrlock
}
