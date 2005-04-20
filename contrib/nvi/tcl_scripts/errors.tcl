#	@(#)errors.tcl	8.2 (Berkeley) 11/18/95
#
# File: errors.tcl
#
# Author: George V. Neville-Neil
#
# Purpose: This file contains vi/tcl code that allows a vi user to parse
# compiler errors and warnings from a make.out file.

proc findErr {} {
	global errScreen
	global currFile
	global fileScreen
	set errLine [lindex [viGetCursor $errScreen] 0]
	set currLine [split [viGetLine $errScreen $errLine] :]
	set currFile [lindex $currLine 0]
	set fileScreen [viNewScreen $errScreen $currFile]
	viSetCursor $fileScreen [lindex $currLine 1] 1
	viMapKey $viScreenId  nextErr
}

proc nextErr {} {
	global errScreen
	global fileScreen
	global currFile
	set errLine [lindex [viGetCursor $errScreen] 0]
	set currLine [split [viGetLine $errScreen $errLine] :]
	if {[string match $currFile [lindex $currLine 0]]} {
		viSetCursor $fileScreen [lindex $currLine 1] 0
		viSwitchScreen $fileScreen
	} else {
		viEndScreen $fileScreen
		set currFile [lindex $currLine 0]
		set fileScreen[viNewScreen $errScreen $currFile]
		viSetCursor $fileScreen [lindex $currLine 1] 0
	}
}

proc initErr {} {
	global viScreenId
	global errScreen
	set errScreen [viNewScreen $viScreenId make.out]
	viMapKey $viScreenId  findErr
}
