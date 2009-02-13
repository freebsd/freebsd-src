#	@(#)gnats.tcl	8.2 (Berkeley) 11/18/95
#
proc init {catFile} {
	global viScreenId
	global categories
	set categories {}
        set categoriesFile [open $catFile r]
	while {[gets $categoriesFile line] >= 0} {
		lappend categories $line
	}
	close $categoriesFile
	viMsg $viScreenId $categories
	viMapKey $viScreenId  next
}

proc next {} {
	global viScreenId
	set cursor [viGetCursor $viScreenId]
	set lineNum [lindex $cursor 0]
	set line [viGetLine $viScreenId $lineNum]
	viMsg $viScreenId [lindex $line 0]
	if {[lindex $line 0] == ">Confidential:"} {
		confNext $lineNum $line
	} elseif {[lindex $line 0] == ">Severity:"} {
		sevNext $lineNum $line
	} elseif {[lindex $line 0] == ">Priority:"} {
		priNext $lineNum $line
	} elseif {[lindex $line 0] == ">Class:"} {
		classNext $lineNum $line
	} elseif {[lindex $line 0] == ">Category:"} {
		catNext $lineNum $line
	}
}

proc confNext {lineNum line} {
	global viScreenId
	viMsg $viScreenId [lindex $line 1]
	if {[lindex $line 1] == "yes"} {
		viSetLine $viScreenId $lineNum ">Confidential: no"
	} else {
		viSetLine $viScreenId $lineNum ">Confidential: yes"
	}
}

proc sevNext {lineNum line} {
	global viScreenId
	viMsg $viScreenId [lindex $line 1]
	if {[lindex $line 1] == "non-critical"} {
		viSetLine $viScreenId $lineNum ">Severity: serious"
	} elseif {[lindex $line 1] == "serious"} {
		viSetLine $viScreenId $lineNum ">Severity: critical"
	} elseif {[lindex $line 1] == "critical"} {
		viSetLine $viScreenId $lineNum ">Severity: non-critical"
	}
}

proc priNext {lineNum line} {
	global viScreenId
	viMsg $viScreenId [lindex $line 1]
	if {[lindex $line 1] == "low"} {
		viSetLine $viScreenId $lineNum ">Priority: medium"
	} elseif {[lindex $line 1] == "medium"} {
		viSetLine $viScreenId $lineNum ">Priority: high"
	} elseif {[lindex $line 1] == "high"} {
		viSetLine $viScreenId $lineNum ">Priority: low"
	}
}

proc classNext {lineNum line} {
	global viScreenId
	viMsg $viScreenId [lindex $line 1]
	if {[lindex $line 1] == "sw-bug"} {
		viSetLine $viScreenId $lineNum ">Class: doc-bug"
	} elseif {[lindex $line 1] == "doc-bug"} {
		viSetLine $viScreenId $lineNum ">Class: change-request"
	} elseif {[lindex $line 1] == "change-request"} {
		viSetLine $viScreenId $lineNum ">Class: support"
	} elseif {[lindex $line 1] == "support"} {
		viSetLine $viScreenId $lineNum ">Class: sw-bug"
	}
}

proc catNext {lineNum line} {
	global viScreenId
	global categories
	viMsg $viScreenId [lindex $line 1]
	set curr [lsearch -exact $categories [lindex $line 1]]
	if {$curr == -1} {
		set curr 0
	}
	viMsg $viScreenId $curr
	viSetLine $viScreenId $lineNum ">Class: [lindex $categories $curr]"
}

init abekas
