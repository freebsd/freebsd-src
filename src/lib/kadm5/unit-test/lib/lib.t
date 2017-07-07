global timeout
set timeout 60

set lib_pid 0

#
# The functions in this library used to be responsible for bazillions
# of wasted api_starts.  Now, they all just use their own library
# handle so they are not interrupted when the main tests call init or
# destroy.  They have to keep track of when the api exists and
# restarts, though, since the lib_handle needs to be re-opened in that
# case.
#
proc lib_start_api {} {
    global spawn_id lib_pid test

    if {! [api_isrunning $lib_pid]} {
	api_exit
	set lib_pid [api_start]
	if {! [cmd {
	    kadm5_init admin admin $KADM5_ADMIN_SERVICE null \
		    $KADM5_STRUCT_VERSION $KADM5_API_VERSION_3 \
		    lib_handle
	}]} {
	    perror "$test: unexpected failure in init"
	    return
	}
	verbose "+++ restarted api ($lib_pid) for lib"
    } else {
	verbose "+++ api $lib_pid already running for lib"
    }	
}

proc cmd {command} {
    global prompt
    global spawn_id
    global test

    send "[string trim $command]\n"
    expect {
	-re "OK .*$prompt$" { return 1 }
        -re "ERROR .*$prompt$" { return 0 }
	"wrong # args" { perror "$test: wrong number args"; return 0 }
        timeout { fail "$test: timeout"; return 0 }
        eof { fail "$test: eof"; api_exit; lib_start_api; return 0 }
    }
}

proc tcl_cmd {command} {
    global prompt spawn_id test

    send "[string trim $command]\n"
    expect {
	-re "$prompt$" { return 1}
	"wrong # args" { perror "$test: wrong number args"; return 0 }
	timeout { error_and_restart "timeout" }
	eof { api_exit; lib_start_api; return 0 }
    }
}

proc one_line_succeed_test {command} {
    global prompt
    global spawn_id
    global test

    send "[string trim $command]\n"
    expect {
	-re "OK .*$prompt$"		{ pass "$test"; return 1 }
	-re "ERROR .*$prompt$" { 
		fail "$test: $expect_out(buffer)"; return 0
	}
	"wrong # args" { perror "$test: wrong number args"; return 0 }
	timeout				{ fail "$test: timeout"; return 0 }
	eof				{ fail "$test: eof"; api_exit; lib_start_api; return 0 }
    }
}

proc one_line_fail_test {command code} {
    global prompt
    global spawn_id
    global test

    send "[string trim $command]\n"
    expect {
	-re "ERROR .*$code.*$prompt$"	{ pass "$test"; return 1 }
	-re "ERROR .*$prompt$"	{ fail "$test: bad failure"; return 0 }
	-re "OK .*$prompt$"		{ fail "$test: bad success"; return 0 }
	"wrong # args" { perror "$test: wrong number args"; return 0 }
	timeout				{ fail "$test: timeout"; return 0 }
	eof				{ fail "$test: eof"; api_exit; lib_start_api; return 0 }
    }
}

proc one_line_fail_test_nochk {command} {
    global prompt
    global spawn_id
    global test

    send "[string trim $command]\n"
    expect {
	-re "ERROR .*$prompt$"	{ pass "$test:"; return 1 }
	-re "OK .*$prompt$"		{ fail "$test: bad success"; return 0 }
	"wrong # args" { perror "$test: wrong number args"; return 0 }
	timeout				{ fail "$test: timeout"; return 0 }
	eof				{ fail "$test: eof"; api_exit; lib_start_api; return 0 }
    }
}

proc resync {} {
    global prompt spawn_id test

    expect {
	-re "$prompt$"	{}
	"wrong # args" { perror "$test: wrong number args"; return 0 }
	eof { api_exit; lib_start_api }
    }
}

proc create_principal {name} {
    lib_start_api

    set ret [cmd [format {
	kadm5_create_principal $lib_handle [simple_principal \
		"%s"] {KADM5_PRINCIPAL} "%s"
    } $name $name]]

    return $ret
}

proc create_policy {name} {
    lib_start_api

    set ret [cmd [format {
	    kadm5_create_policy $lib_handle [simple_policy "%s"] \
		    {KADM5_POLICY}
	} $name $name]]

    return $ret
}

proc create_principal_pol {name policy} {
    lib_start_api

    set ret [cmd [format {
	    kadm5_create_principal $lib_handle [princ_w_pol "%s" \
		    "%s"] {KADM5_PRINCIPAL KADM5_POLICY} "%s"
    } $name $policy $name]]

    return $ret
}

proc delete_principal {name} {
    lib_start_api

    set ret [cmd [format {
	    kadm5_delete_principal $lib_handle "%s"
    } $name]]

    return $ret
}

proc delete_policy {name} {
    lib_start_api

    set ret [cmd [format {kadm5_delete_policy $lib_handle "%s"} $name]]

    return $ret
}

proc principal_exists {name} {
#    puts stdout "Starting principal_exists."

    lib_start_api

    set ret [cmd [format {
	kadm5_get_principal $lib_handle "%s" principal \
	  KADM5_PRINCIPAL_NORMAL_MASK
    } $name]]

#   puts stdout "Finishing principal_exists."

    return $ret
}

proc policy_exists {name} {
    lib_start_api

#    puts stdout "Starting policy_exists."

    set ret [cmd [format {
	    kadm5_get_policy $lib_handle "%s" policy
	} $name]]

#    puts stdout "Finishing policy_exists."

    return $ret
}

proc error_and_restart {error} {
    api_exit
    api_start
    perror $error
}

proc test {name} {
   global test verbose

   set test $name
   if {$verbose >= 1} {
	puts stdout "At $test"
   }
}

proc begin_dump {} {
    global TOP
    global RPC
    
    if { ! $RPC } {
#	exec $env(SIMPLE_DUMP) > /tmp/dump.before
    }
}

proc end_dump_compare {name} {
    global  file
    global  TOP
    global  RPC

    if { ! $RPC } { 
#	set file $TOP/admin/lib/unit-test/diff-files/$name
#	exec $env(SIMPLE_DUMP) > /tmp/dump.after
#	exec $env(COMPARE_DUMP) /tmp/dump.before /tmp/dump.after $file
    }
}

proc kinit { princ pass {opts ""} } {
	global env;
        global KINIT

	eval spawn $KINIT -5 $opts $princ
	expect {
		-re {Password for .*: $}
		    {send "$pass\n"}
		timeout {puts "Timeout waiting for prompt" ; close }
	}

	# this necessary so close(1) in the child will not sleep waiting for
	# the parent, which is us, to read pending data.

	expect {
		"when initializing cache" { perror "kinit failed: $expect_out(buffer)" }
		eof {}
	}
	wait
}

proc kdestroy {} {
        global KDESTROY
	global errorCode errorInfo
	global env

	if {[info exists errorCode]} {
		set saveErrorCode $errorCode
	}
	if {[info exists errorInfo]} {
		set saveErrorInfo $errorInfo
	}
	catch "system $KDESTROY -5 2>/dev/null"
	if {[info exists saveErrorCode]} {
		set errorCode $saveErrorCode
	} elseif {[info exists errorCode]} {
		unset errorCode
	}
	if {[info exists saveErrorInfo]} {
		set errorInfo $saveErrorInfo
	} elseif {[info exists errorInfo]} {
		unset errorInfo
	}
}

proc create_principal_with_keysalts {name keysalts} {
    global kadmin_local

    spawn $kadmin_local -e "$keysalts"
    expect {
	"kadmin.local:" {}
	default { perror "waiting for kadmin.local prompt"; return 1}
    }
    send "ank -pw \"$name\" \"$name\"\n"
    expect {
	-re "Principal \"$name.*\" created." {}
	"kadmin.local:" {
	    perror "expecting principal created message"; 
	    return 1
	}
	default { perror "waiting for principal created message"; return 1 }
    }
    expect {
	"kadmin.local:" {}
	default { perror "waiting for kadmin.local prompt"; return 1 }
    }
    close
    wait
    return 0
}

    
