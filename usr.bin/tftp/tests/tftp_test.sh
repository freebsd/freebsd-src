#
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

tftp_dir="${TMPDIR:-/tmp}/tftp.dir"
inetd_conf="${TMPDIR:-/tmp}/inetd.conf"
inetd_pid="${TMPDIR:-/tmp}/inetd.pid"

start_tftpd() {
	if ! [ -z "$(sockstat -PUDP -p69 -q)" ] ; then
		atf_skip "the tftp port is in use"
	fi
	echo "starting inetd for $(atf_get ident)" >&2
	rm -rf "${tftp_dir}"
	mkdir "${tftp_dir}"
	cat >"${inetd_conf}" <<EOF
tftp	dgram	udp	wait	root	/usr/libexec/tftpd	tftpd -d15 -l ${tftp_dir}
tftp	dgram	udp6	wait	root	/usr/libexec/tftpd	tftpd -d15 -l ${tftp_dir}
EOF
	/usr/sbin/inetd -a localhost -p "${inetd_pid}" "${inetd_conf}"
}

stop_tftpd() {
	echo "stopping inetd for $(atf_get ident)" >&2
	# Send SIGTERM to inetd, then SIGKILL until it's gone
	local sig=TERM
	while pkill -$sig -LF "${inetd_pid}" inetd ; do
		echo "waiting for inetd to stop" >&2
		sleep 1
		sig=KILL
	done
	rm -rf "${tftp_dir}" "${inetd_conf}" "${inetd_pid}"
}

atf_test_case tftp_get_big cleanup
tftp_get_big_head() {
	atf_set "descr" "get command with big file"
	atf_set "require.user" "root"
}
tftp_get_big_body() {
	start_tftpd
	local remote_file="${tftp_dir}/remote.bin"
	dd if=/dev/urandom of="${remote_file}" bs=1m count=16 status=none
	local local_file="local.bin"
	echo "get ${remote_file##*/} ${local_file}" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp localhost <client-script
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_get_big_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_host cleanup
tftp_get_host_head() {
	atf_set "descr" "get command with host name"
	atf_set "require.user" "root"
}
tftp_get_host_body() {
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	echo "get localhost:${remote_file##*/}" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp <client-script
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_get_host_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_ipv4 cleanup
tftp_get_ipv4_head() {
	atf_set "descr" "get command with ipv4 address"
	atf_set "require.user" "root"
}
tftp_get_ipv4_body() {
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	echo "get 127.0.0.1:${remote_file##*/}" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp <client-script
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_get_ipv4_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_ipv6 cleanup
tftp_get_ipv6_head() {
	atf_set "descr" "get command with ipv6 address"
	atf_set "require.user" "root"
}
tftp_get_ipv6_body() {
	sysctl -q kern.features.inet6 || atf_skip "This test requires IPv6 support"
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	echo "get [::1]:${remote_file##*/}" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp <client-script
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_get_ipv6_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_one cleanup
tftp_get_one_head() {
	atf_set "descr" "get command with one argument"
	atf_set "require.user" "root"
}
tftp_get_one_body() {
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	echo "get ${remote_file##*/}" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp localhost <client-script
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_get_one_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_two cleanup
tftp_get_two_head() {
	atf_set "descr" "get command with two arguments"
	atf_set "require.user" "root"
}
tftp_get_two_body() {
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="world.txt"
	echo "get ${remote_file##*/} ${local_file}" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp localhost <client-script
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_get_two_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_more cleanup
tftp_get_more_head() {
	atf_set "descr" "get command with three or more arguments"
	atf_set "require.user" "root"
}
tftp_get_more_body() {
	start_tftpd
	for n in 3 4 5 6 7 ; do
		echo -n "get" >client-script
		for f in $(jot -c $n 97) ; do
			echo "test file $$/$f/$n" >"${tftp_dir}/${f}.txt"
			echo -n " ${f}.txt" >>client-script
			rm -f "${f}.txt"
		done
		echo >>client-script
		atf_check -o match:"Received [0-9]+ bytes" \
		    tftp localhost <client-script
		for f in $(jot -c $n 97) ; do
			atf_check cmp -s "${f}.txt" "${tftp_dir}/${f}.txt"
		done
	done
}
tftp_get_more_cleanup() {
	stop_tftpd
}

atf_test_case tftp_get_multi_host cleanup
tftp_get_multi_host_head() {
	atf_set "descr" "get command with multiple files and host name"
	atf_set "require.user" "root"
}
tftp_get_multi_host_body() {
	start_tftpd
	for f in a b c ; do
		echo "test file $$/$f/$n" >"${tftp_dir}/${f}.txt"
		rm -f "${f}.txt"
	done
	echo "get localhost:a.txt b.txt c.txt" >client-script
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp localhost <client-script
	for f in a b c ; do
		atf_check cmp -s "${f}.txt" "${tftp_dir}/${f}.txt"
	done
}
tftp_get_multi_host_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_big cleanup
tftp_put_big_head() {
	atf_set "descr" "put command with big file"
	atf_set "require.user" "root"
}
tftp_put_big_body() {
	start_tftpd
	local local_file="local.bin"
	dd if=/dev/urandom of="${local_file}" bs=1m count=16 status=none
	local remote_file="${tftp_dir}/random.bin"
	truncate -s 0 "${remote_file}"
	chown nobody:nogroup "${remote_file}"
	chmod 0666 "${remote_file}"
	echo "put ${local_file} ${remote_file##*/}" >client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp localhost <client-script
	atf_check cmp -s "${remote_file}" "${local_file}"
}
tftp_put_big_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_host cleanup
tftp_put_host_head() {
	atf_set "descr" "put command with host name"
	atf_set "require.user" "root"
}
tftp_put_host_body() {
	start_tftpd
	local local_file="local.txt"
	echo "test file $$" >"${local_file}"
	local remote_file="${tftp_dir}/remote.txt"
	truncate -s 0 "${remote_file}"
	chown nobody:nogroup "${remote_file}"
	chmod 0666 "${remote_file}"
	echo "put ${local_file} localhost:${remote_file##*/}" >client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp <client-script
	atf_check cmp -s "${remote_file}" "${local_file}"
}
tftp_put_host_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_ipv4 cleanup
tftp_put_ipv4_head() {
	atf_set "descr" "put command with ipv4 address"
	atf_set "require.user" "root"
}
tftp_put_ipv4_body() {
	start_tftpd
	local local_file="local.txt"
	echo "test file $$" >"${local_file}"
	local remote_file="${tftp_dir}/remote.txt"
	truncate -s 0 "${remote_file}"
	chown nobody:nogroup "${remote_file}"
	chmod 0666 "${remote_file}"
	echo "put ${local_file} 127.0.0.1:${remote_file##*/}" >client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp <client-script
	atf_check cmp -s "${remote_file}" "${local_file}"
}
tftp_put_ipv4_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_ipv6 cleanup
tftp_put_ipv6_head() {
	atf_set "descr" "put command with ipv6 address"
	atf_set "require.user" "root"
}
tftp_put_ipv6_body() {
	sysctl -q kern.features.inet6 || atf_skip "This test requires IPv6 support"
	start_tftpd
	local local_file="local.txt"
	echo "test file $$" >"${local_file}"
	local remote_file="${tftp_dir}/remote.txt"
	truncate -s 0 "${remote_file}"
	chown nobody:nogroup "${remote_file}"
	chmod 0666 "${remote_file}"
	echo "put ${local_file} [::1]:${remote_file##*/}" >client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp <client-script
	atf_check cmp -s "${remote_file}" "${local_file}"
}
tftp_put_ipv6_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_one cleanup
tftp_put_one_head() {
	atf_set "descr" "put command with one argument"
	atf_set "require.user" "root"
}
tftp_put_one_body() {
	start_tftpd
	local local_file="file.txt"
	echo "test file $$" >"${local_file}"
	local remote_file="${tftp_dir}/${local_file}"
	truncate -s 0 "${remote_file}"
	chown nobody:nogroup "${remote_file}"
	chmod 0666 "${remote_file}"
	echo "put ${local_file}" >client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp localhost <client-script
	atf_check cmp -s "${remote_file}" "${local_file}"
}
tftp_put_one_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_two cleanup
tftp_put_two_head() {
	atf_set "descr" "put command with two arguments"
	atf_set "require.user" "root"
}
tftp_put_two_body() {
	start_tftpd
	local local_file="local.txt"
	echo "test file $$" >"${local_file}"
	local remote_file="${tftp_dir}/remote.txt"
	truncate -s 0 "${remote_file}"
	chown nobody:nogroup "${remote_file}"
	chmod 0666 "${remote_file}"
	echo "put ${local_file} ${remote_file##*/}" >client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp localhost <client-script
	atf_check cmp -s "${remote_file}" "${local_file}"
}
tftp_put_two_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_more cleanup
tftp_put_more_head() {
	atf_set "descr" "put command with three or more arguments"
	atf_set "require.user" "root"
}
tftp_put_more_body() {
	start_tftpd
	mkdir "${tftp_dir}/subdir"
	for n in 2 3 4 5 6 ; do
		echo -n "put" >client-script
		for f in $(jot -c $n 97) ; do
			echo "test file $$/$f/$n" >"${f}.txt"
			truncate -s 0 "${tftp_dir}/subdir/${f}.txt"
			chown nobody:nogroup "${tftp_dir}/subdir/${f}.txt"
			chmod 0666 "${tftp_dir}/subdir/${f}.txt"
			echo -n " ${f}.txt" >>client-script
		done
		echo " subdir" >>client-script
		atf_check -o match:"Sent [0-9]+ bytes" \
		    tftp localhost <client-script
		for f in $(jot -c $n 97) ; do
			atf_check cmp -s "${tftp_dir}/subdir/${f}.txt" "${f}.txt"
		done
	done
}
tftp_put_more_cleanup() {
	stop_tftpd
}

atf_test_case tftp_put_multi_host cleanup
tftp_put_multi_host_head() {
	atf_set "descr" "put command with multiple files and host name"
	atf_set "require.user" "root"
}
tftp_put_multi_host_body() {
	start_tftpd
	mkdir "${tftp_dir}/subdir"
	echo -n "put" >client-script
	for f in a b c ; do
		echo "test file $$/$f" >"${f}.txt"
		truncate -s 0 "${tftp_dir}/subdir/${f}.txt"
		chown nobody:nogroup "${tftp_dir}/subdir/${f}.txt"
		chmod 0666 "${tftp_dir}/subdir/${f}.txt"
		echo -n " ${f}.txt" >>client-script
	done
	echo " localhost:subdir" >>client-script
	atf_check -o match:"Sent [0-9]+ bytes" \
	    tftp <client-script
	for f in a b c ; do
		atf_check cmp -s "${tftp_dir}/subdir/${f}.txt" "${f}.txt"
	done
}
tftp_put_multi_host_cleanup() {
	stop_tftpd
}

atf_test_case tftp_url_host cleanup
tftp_url_host_head() {
	atf_set "descr" "URL with hostname"
	atf_set "require.user" "root"
}
tftp_url_host_body() {
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp tftp://localhost/"${remote_file##*/}"
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_url_host_cleanup() {
	stop_tftpd
}

atf_test_case tftp_url_ipv4 cleanup
tftp_url_ipv4_head() {
	atf_set "descr" "URL with IPv4 address"
	atf_set "require.user" "root"
}
tftp_url_ipv4_body() {
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp tftp://127.0.0.1/"${remote_file##*/}"
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_url_ipv4_cleanup() {
	stop_tftpd
}

atf_test_case tftp_url_ipv6 cleanup
tftp_url_ipv6_head() {
	atf_set "descr" "URL with IPv6 address"
	atf_set "require.user" "root"
}
tftp_url_ipv6_body() {
	sysctl -q kern.features.inet6 || atf_skip "This test requires IPv6 support"
	atf_expect_fail "tftp does not support bracketed IPv6 literals in URLs"
	start_tftpd
	local remote_file="${tftp_dir}/hello.txt"
	echo "Hello, $$!" >"${remote_file}"
	local local_file="${remote_file##*/}"
	atf_check -o match:"Received [0-9]+ bytes" \
	    tftp tftp://"[::1]"/"${remote_file##*/}"
	atf_check cmp -s "${local_file}" "${remote_file}"
}
tftp_url_ipv6_cleanup() {
	stop_tftpd
}

atf_init_test_cases() {
	atf_add_test_case tftp_get_big
	atf_add_test_case tftp_get_host
	atf_add_test_case tftp_get_ipv4
	atf_add_test_case tftp_get_ipv6
	atf_add_test_case tftp_get_one
	atf_add_test_case tftp_get_two
	atf_add_test_case tftp_get_more
	atf_add_test_case tftp_get_multi_host
	atf_add_test_case tftp_put_big
	atf_add_test_case tftp_put_host
	atf_add_test_case tftp_put_ipv4
	atf_add_test_case tftp_put_ipv6
	atf_add_test_case tftp_put_one
	atf_add_test_case tftp_put_two
	atf_add_test_case tftp_put_more
	atf_add_test_case tftp_put_multi_host
	atf_add_test_case tftp_url_host
	atf_add_test_case tftp_url_ipv4
	atf_add_test_case tftp_url_ipv6
}
