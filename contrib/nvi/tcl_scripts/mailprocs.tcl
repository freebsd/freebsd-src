#	@(#)mailprocs.tcl	8.3 (Berkeley) 4/29/96
#
proc validLine {} {
	global viScreenId
	set line [viGetLine $viScreenId [lindex [viGetCursor $viScreenId] 0]]
	if {[string compare [lindex [split $line :] 0]	"To"] == 0} {
		set addrs [lindex [split $line :] 1]
		foreach name [split $addrs ,] {
			isValid [string trim $name]
		}
	}
}

proc valid {target} {
	set found 0
	set aliasFile [open "~/Mail/aliases" r]
	while {[gets $aliasFile line] >= 0} {
		set name [lindex [split $line :] 0]
		set address [lindex [split $line :] 1]
		if {[string compare $target $name] == 0} {
			set found 1
			break
		}
	}
	close $aliasFile
	if {$found == 1} {
		return $address
	} else {
		return $found
	}
}

proc isValid {target} {
	global viScreenId
	set address [valid $target]
	if {$address != 0} {
		viMsg $viScreenId "$target is [string trim $address]"
	} else {
		viMsg $viScreenId "$target not found"
	}
}

proc isAliasedLine {} {
	global viScreenId
	set line [viGetLine $viScreenId [lindex [viGetCursor $viScreenId] 0]]
	if {[string match [lindex [split $line :] 0] "*To"] == 0} {
		set addrs [lindex [split $line :] 1]
		foreach name [split $addrs ,] {
			isAliased [string trim $name]
		}
	}
}

proc aliased {target} {
	set found 0
	set aliasFile [open "~/Mail/aliases" r]
	while {[gets $aliasFile line] >= 0} {
		set name [lindex [split $line :] 0]
		set address [lindex [split $line :] 1]
		if {[string compare $target [string trim $address]] == 0} {
			set found 1
			break
		}
	}
	close $aliasFile

	return $found
}

proc isAliased {target} {
	global viScreenId
	set found [aliased $target]

	if {$found} {
		viMsg $viScreenId "$target is aliased to [string trim $name]"
	} else {
		viMsg $viScreenId "$target not aliased"
	}
}

proc appendAlias {target address} {
	if {![aliased $target]} {
		set aliasFile [open "~/Mail/aliases" a]
		puts $aliasFile "$target: $address"
	}
	close $aliasFile
}

proc expand {} {
	global viScreenId
	set row [lindex [viGetCursor $viScreenId] 0]]
	set column [lindex [viGetCursor $viScreenId] 1]]
	set line [viGetLine $viScreenId $row]
	while {$column < [string length $line] && \
		[string index $line $column] != ' '} {
		append $target [string index $line $column]
		incr $column
	}
	set found [isValid $target]
}

proc cite {} {
	global viScreenId
	global viStartLine
	global viStopLine
	for {set i $viStartLine} {$i <= $viStopLine} {incr i} {
		set newLine "> "
		append newLine [viGetLine $viScreenId $i]
		viSetLine $viScreenId $i $newLine
	}
}

global viScreenId
viMapKey $viScreenId  isAliasedLine
viMapKey $viScreenId  validLine
