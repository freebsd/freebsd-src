#!/bin/sh
#
#

uidrange="60000:100000"
gidrange="60000:100000"
uidinrange="nobody"
uidoutrange="daemon"
gidinrange="nobody" # We expect $uidinrange in this group
gidoutrange="daemon" # We expect $uidinrange in this group


check_ko()
{
	if ! sysctl -N security.mac.bsdextended >/dev/null 2>&1; then
		atf_skip "mac_bsdextended(4) support isn't available"
	fi
	if [ $(sysctl -n security.mac.bsdextended.enabled) = "0" ]; then
		# The kernel module is loaded but disabled.  Enable it for the
		# duration of the test.
		touch enabled_bsdextended
		sysctl security.mac.bsdextended.enabled=1
	fi
}

setup()
{
	check_ko
	mkdir mnt
	mdmfs -s 25m md mnt \
		|| atf_fail "failed to mount md device"
	chmod a+rwx mnt
	md_device=$(mount -p | grep "$PWD/mnt" | awk '{ gsub(/^\/dev\//, "", $1); print $1 }')
	if [ -z "$md_device" ]; then
		atf_fail "md device not properly attached to the system"
	fi
	echo $md_device > md_device

	ugidfw remove 1

	cat > mnt/test-script.sh <<'EOF'
#!/bin/sh
: > $1
EOF
	if [ $? -ne 0 ]; then
		atf_fail "failed to create test script"
	fi

	file1=mnt/test-$uidinrange
	file2=mnt/test-$uidoutrange
	command1="sh mnt/test-script.sh $file1"
	command2="sh mnt/test-script.sh $file2"

	# $uidinrange file
	atf_check -s exit:0 su -m $uidinrange -c "$command1"

	chown "$uidinrange":"$gidinrange" $file1
	chmod a+w $file1

	# $uidoutrange file
	if ! $command2; then
		atf_fail $desc
	fi

	chown "$uidoutrange":"$gidoutrange" $file2
	chmod a+w $file2
}

cleanup()
{
	ugidfw remove 1

	umount -f mnt
	if [ -f md_device ]; then
		mdconfig -d -u $( cat md_device )
	fi
	if [ -f enabled_bsdextended ]; then
		sysctl security.mac.bsdextended.enabled=0
	fi
}

atf_test_case no_rules cleanup
no_rules_head()
{
	atf_set "require.user" "root"
}
no_rules_body()
{
	setup

	# no rules $uidinrange
	atf_check -s exit:0 su -fm $uidinrange -c "$command1"

	# no rules $uidoutrange
	atf_check -s exit:0 su -fm $uidoutrange -c "$command1"
}
no_rules_cleanup()
{
	cleanup
}

atf_test_case subject_match_on_uid cleanup
subject_match_on_uid_head()
{
	atf_set "require.user" "root"
}
subject_match_on_uid_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object mode rasx
	# subject uid in range
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"

	# subject uid out range
	atf_check -s exit:0 su -fm $uidoutrange -c "$command1"

}
subject_match_on_uid_cleanup()
{
	cleanup
}

atf_test_case subject_match_on_gid cleanup
subject_match_on_gid_head()
{
	atf_set "require.user" "root"
}
subject_match_on_gid_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject gid $gidrange object mode rasx

	# subject gid in range
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"

	# subject gid out range
	atf_check -s exit:0 su -fm $uidoutrange -c "$command1"
}
subject_match_on_gid_cleanup()
{
	cleanup
}

atf_test_case subject_match_on_jail cleanup
subject_match_on_jail_head()
{
	atf_set "require.progs" "jail"
	atf_set "require.user" "root"
}
subject_match_on_jail_body()
{
	setup

	atf_expect_fail "this testcase fails (see bug # 205481)"
	# subject matching jailid
	jailid=`jail -i / localhost 127.0.0.1 /usr/sbin/daemon -f /bin/sh -c "(sleep 5; touch mnt/test-jail) &"`
	atf_check -s exit:0 ugidfw set 1 subject jailid $jailid object mode rasx
	sleep 10

	if [ -f mnt/test-jail ]; then
		atf_fail "$desc"
	fi

	rm -f mnt/test-jail
	# subject nonmatching jailid
	jailid=`jail -i / localhost 127.0.0.1 /usr/sbin/daemon -f /bin/sh -c "(sleep 5; touch mnt/test-jail) &"`
	sleep 10
	if ! [ -f mnt/test-jail ]; then
		atf_fail $desc
	fi
}
subject_match_on_jail_cleanup()
{
	cleanup
}

atf_test_case object_uid cleanup
object_uid_head()
{
	atf_set "require.user" "root"
}
object_uid_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject object uid $uidrange mode rasx

	# object uid in range
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"

	# object uid out range
	atf_check -s exit:0 su -fm $uidinrange -c "$command2"
	atf_check -s exit:0 ugidfw set 1 subject object uid $uidrange mode rasx

	# object uid in range (different subject)
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidoutrange -c "$command1"

	# object uid out range (different subject)
	atf_check -s exit:0 su -fm $uidoutrange -c "$command2"

}
object_uid_cleanup()
{
	cleanup
}

atf_test_case object_gid cleanup
object_gid_head()
{
	atf_set "require.user" "root"
}
object_gid_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject object gid $uidrange mode rasx

	# object gid in range
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"

	# object gid out range
	atf_check -s exit:0 su -fm $uidinrange -c "$command2"
	# object gid in range (different subject)
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidoutrange -c "$command1"

	# object gid out range (different subject)
	atf_check -s exit:0 su -fm $uidoutrange -c "$command2"
}
object_gid_cleanup()
{
	cleanup
}

atf_test_case object_filesys cleanup
object_filesys_head()
{
	atf_set "require.user" "root"
}
object_filesys_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object filesys / mode rasx
	# object out of filesys
	atf_check -s exit:0 su -fm $uidinrange -c "$command1"

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object filesys mnt mode rasx
	# object in filesys
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"
}
object_filesys_cleanup()
{
	cleanup
}

atf_test_case object_suid cleanup
object_suid_head()
{
	atf_set "require.user" "root"
}
object_suid_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object suid mode rasx
	# object notsuid
	atf_check -s exit:0 su -fm $uidinrange -c "$command1"

	chmod u+s $file1
	# object suid
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"
	chmod u-s $file1

}
object_suid_cleanup()
{
	cleanup
}

atf_test_case object_sgid cleanup
object_sgid_head()
{
	atf_set "require.user" "root"
}
object_sgid_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object sgid mode rasx
	# object notsgid
	atf_check -s exit:0 su -fm $uidinrange -c "$command1"

	chmod g+s $file1
	# object sgid
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"
	chmod g-s $file1
}
object_sgid_cleanup()
{
	cleanup
}

atf_test_case object_uid_matches_subject cleanup
object_uid_matches_subject_head()
{
	atf_set "require.user" "root"
}
object_uid_matches_subject_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object uid_of_subject mode rasx

	# object uid notmatches subject
	atf_check -s exit:0 su -fm $uidinrange -c "$command2"

	# object uid matches subject
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"
}
object_uid_matches_subject_cleanup()
{
	cleanup
}

atf_test_case object_gid_matches_subject cleanup
object_gid_matches_subject_head()
{
	atf_set "require.user" "root"
}
object_gid_matches_subject_body()
{
	setup

	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object gid_of_subject mode rasx

	# object gid notmatches subject
	atf_check -s exit:0 su -fm $uidinrange -c "$command2"

	# object gid matches subject
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"

}
object_gid_matches_subject_cleanup()
{
	cleanup
}

atf_test_case object_type cleanup
object_type_head()
{
	atf_set "require.user" "root"
}
object_type_body()
{
	setup

	# object not type
	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object type dbclsp mode rasx
	atf_check -s exit:0 su -fm $uidinrange -c "$command1"

	# object type
	atf_check -s exit:0 ugidfw set 1 subject uid $uidrange object type r mode rasx
	atf_check -s not-exit:0 -e match:"Permission denied" \
		su -fm $uidinrange -c "$command1"
}
object_type_cleanup()
{
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case no_rules
	atf_add_test_case subject_match_on_uid
	atf_add_test_case subject_match_on_gid
	atf_add_test_case subject_match_on_jail
	atf_add_test_case object_uid
	atf_add_test_case object_gid
	atf_add_test_case object_filesys
	atf_add_test_case object_suid
	atf_add_test_case object_sgid
	atf_add_test_case object_uid_matches_subject
	atf_add_test_case object_gid_matches_subject
	atf_add_test_case object_type
}
