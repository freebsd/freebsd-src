#!/usr/bin/tclsh
# Copyright (c) 1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# addgroup - add a group or add users to a group
#
# addgroup [-g gid] group [user[,user,...]]
#
#
# addgroup -g 2000 foobar
#
# Add group `foobar' to group database. Group id is 2000 if 
# possible or higher. Don't add group `foobar' if `foobar' is
# already in group database. 
#
#
# addgroup foo blech,bar
#
# Add user `blech' and user `bar' to group `foo'. Create group
# `foo' with default gid if not exists.
#
#
# The option [-g gid] is only for new groups. 
#
# see group(5)
#
# TODO: 
# 	file locking
#	signal handling
# 	add only users who exist
#
# $Id: addgroup.tcl,v 1.1 1996/10/29 20:31:43 wosch Exp $

# set global variables
set etc_group "/etc/group";        #set etc_group "/usr/tmp/group" 
set gid_start 1000
set gid_max 65500

proc putsErr {string} {
    if {[catch {open "/dev/stderr" w} stderr]} {
	puts $stderr
    } else {
	puts $stderr $string
	close $stderr
    }
}

proc usage {} {
    putsErr {usage: addgroup group [user]}
    putsErr {       addgroup [-g gid] group [user[,user,...]]}
    exit 1
}

# check double user names: foo,bla,foo
proc double_name {groupmembers} {
    set l [split $groupmembers ","]
    if {[llength $l] > 1} {
	for {set i 0} {$i < [llength $l]} {incr i} {
	    if {[lsearch [lrange $l [expr $i + 1] end] \
                [lindex $l $i]] != -1} {
		putsErr "Double user name: [lindex $l $i]"
		return 1
	    }
	}
    }
    return 0
}

# check group(5) limits
proc group_limit {string} {
    set line_max 1000;      # max group line length
    set groups_max 200;     # max group members

    if {[string length $string] >= $line_max} {
	return 1
    }

    set l [split $string ","]
    if {[llength $l] >= $groups_max} {
	return 1
    }

    return 0
}

# cleanup and die
proc Err {string} {
    upvar etc_group_new new
    putsErr "$string"
    exec rm -f $new
    exit 1
}

if {$argc < 1} { usage }

# check options
switch -glob -- [lindex $argv 0]  { 
    -g* {
	if {$argc < 2} { 
	    putsErr "Missing group id"
	    usage 
	}
	set g [lindex $argv 1]
	if {$g < 100 || $g >= $gid_max} {
	    putsErr "Group id out of range 100 < $g < $gid_max"
	    usage
	}
	set gid_start $g
	incr argc -2
	set argv [lrange $argv 2 end]
    }
    -* { usage }
}

if {$argc < 1} { usage }

# read group name
set groupname [lindex $argv 0]
if {[string match "*:*" $groupname] != 0} {
    putsErr "Colon are not allowed in group name: ``$groupname''"
    usage
}

# read optional group members
if {$argc == 2} {
    set groupmembers [lindex $argv 1]
    if {[string match "*:*" $groupmembers] != 0} {
	putsErr "Colon are not allowed in user names: ``$groupmembers''"
	usage
    }
    if {[double_name $groupmembers] != 0} {
	usage
    }
} else {
    set groupmembers ""
}


# open /etc/group database
if {[catch {open $etc_group r} db]} {
    Err $db
}

# open temporary database
set etc_group_new "$etc_group.new"; 
if {[catch {open $etc_group_new w} db_new]} {
    Err $db_new
}
set done 0

while {[gets $db line] >= 0 } {
    if {$done > 0} {
	puts $db_new $line
	continue
    }

    # ``group:passwd:gid:member''
    #     0      1    2    3 
    set l [split $line ":"]
    set group([lindex $l 0]) [lindex $l 2]
    set gid([lindex $l 2]) [lindex $l 0]
    set member([lindex $l 0]) [lindex $l 3]

    # found existing group
    if {[string compare [lindex $l 0] $groupname] == 0} {
	if {[string compare $groupmembers ""] == 0} {
	    Err "Group exists: ``$groupname''"
	}

	# add new group members
	set y [lindex $l 3]

	# group with no group members?
	if {[string compare $y ""] == 0} {
	    if {[group_limit "$line$groupmembers"] == 0} {
		puts $db_new "$line$groupmembers"
	    } else {
		Err "group line too long: ``$line$groupmembers''"
	    }
	} else {
	    if {[group_limit "$line,$groupmembers"] == 0} {
		if {[double_name "$y,$groupmembers"] != 0} {
		    Err "\t$line,$groupmembers"
		} else {
		    puts $db_new "$line,$groupmembers"
		}
	    } else {
		Err "group line too long: ``$line,$groupmembers''"
	    }
	}
	set done 1
    } else {
	puts $db_new $line
    }
}

# add a new group
if {$done == 0} {
    for {set i $gid_start} {$i < $gid_max} {incr i} {
	if {[info exists gid($i)] == 0} {
	    if {[group_limit "$groupname:*:$i:$groupmembers"] == 0} {
		puts $db_new "$groupname:*:$i:$groupmembers"	    
	    } else {
		Err "group line too long: ``$groupname:*:$i:$groupmembers''"
	    }
	    set done 1
	    break
	}
    }

    # no free group id
    if {$done == 0} {
	Err "Cannot find free group id: ``$groupname''"
    }
}

close $db_new
close $db
exec cp -pf $etc_group "$etc_group.bak"
exec mv -f $etc_group_new $etc_group
