# Public Domain
# Zev Weiss, 2016

tid="AllowUsers/DenyUsers"

me="$LOGNAME"
if [ "x$me" = "x" ]; then
	me=`whoami`
fi
other="nobody"

test_auth()
{
	deny="$1"
	allow="$2"
	should_succeed="$3"
	failmsg="$4"

	start_sshd -oDenyUsers="$deny" -oAllowUsers="$allow"

	${SSH} -F $OBJ/ssh_config "$me@somehost" true
	status=$?

	if (test $status -eq 0 && ! $should_succeed) \
	    || (test $status -ne 0 && $should_succeed); then
		fail "$failmsg"
	fi

	stop_sshd
}

#         DenyUsers     AllowUsers    should_succeed  failure_message
test_auth ""            ""            true            "user in neither DenyUsers nor AllowUsers denied"
test_auth "$other $me"  ""            false           "user in DenyUsers allowed"
test_auth "$me $other"  ""            false           "user in DenyUsers allowed"
test_auth ""            "$other"      false           "user not in AllowUsers allowed"
test_auth ""            "$other $me"  true            "user in AllowUsers denied"
test_auth ""            "$me $other"  true            "user in AllowUsers denied"
test_auth "$me $other"  "$me $other"  false           "user in both DenyUsers and AllowUsers allowed"
test_auth "$other $me"  "$other $me"  false           "user in both DenyUsers and AllowUsers allowed"
