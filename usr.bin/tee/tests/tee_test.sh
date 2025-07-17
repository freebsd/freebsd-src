#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case single_file
single_file_body()
{
	atf_check -o inline:"text\n" -x "echo text | tee file"
	atf_check -o inline:"text\n" cat file
}

atf_test_case device
device_body()
{
	atf_check -e inline:"text\n" -o inline:"text\n" -x \
	    "echo text | tee /dev/stderr"
}

atf_test_case multiple_file
multiple_file_body()
{
	atf_check -o inline:"text\n" -x "echo text | tee file1 file2"
	atf_check -o inline:"text\n" cat file1
	atf_check -o inline:"text\n" cat file2
}

atf_test_case append
append_body()
{
	atf_check -o ignore -x "echo text | tee file"
	atf_check -o inline:"text\n" cat file

	# Should overwrite if done again
	atf_check -o ignore -x "echo text | tee file"
	atf_check -o inline:"text\n" cat file

	# Should duplicate if we use -a
	atf_check -o ignore -x "echo text | tee -a file"
	atf_check -o inline:"text\ntext\n" cat file
}

atf_test_case sigint_ignored
sigint_ignored_head()
{
	# This is most cleanly tested with interactive input, to avoid adding
	# a lot of complexity in trying to manage an input and signal delivery
	# dance purely in shell.
	atf_set "require.progs" "porch"
}
sigint_ignored_body()
{

	# sigint.orch will write "text" to the file twice if we're properly
	# ignoring SIGINT, so we'll do one test to confirm that SIGINT is not
	# being ignored by porch(1), then another to confirm that tee(1) will
	# ignore SIGINT when instructed to.
	atf_check -s exit:1 -e ignore \
	    porch -f $(atf_get_srcdir)/sigint.orch tee file
	atf_check -o inline:"text\n" cat file

	atf_check porch -f $(atf_get_srcdir)/sigint.orch tee -i file
	atf_check -o inline:"text\ntext\n" cat file
}

atf_test_case unixsock "cleanup"
unixsock_pidfile="nc.pid"

unixsock_body()
{
	outfile=out.log

	nc -lU logger.sock > "$outfile" &
	npid=$!

	atf_check -o save:"$unixsock_pidfile" echo "$npid"

	# Wait for the socket to come online, just in case.
	while [ ! -S logger.sock ]; do
		sleep 0.1
	done

	atf_check -o inline:"text over socket\n" -x \
	    'echo "text over socket" | tee logger.sock'

	atf_check rm "$unixsock_pidfile"
	atf_check -o inline:"text over socket\n" cat "$outfile"
}
unixsock_cleanup()
{
	if [ -s "$unixsock_pidfile" ]; then
		read npid < "$unixsock_pidfile"
		kill "$npid"
	fi
}

atf_init_test_cases()
{
	atf_add_test_case single_file
	atf_add_test_case device
	atf_add_test_case multiple_file
	atf_add_test_case append
	atf_add_test_case sigint_ignored
	atf_add_test_case unixsock
}
