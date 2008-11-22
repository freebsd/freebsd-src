#	@(#)wc.tcl	8.2 (Berkeley) 11/18/95
#
proc wc {} {
	global viScreenId
	global viStartLine
	global viStopLine

	set lines [viLastLine $viScreenId]
	set output ""
	set words 0
	for {set i $viStartLine} {$i <= $viStopLine} {incr i} {
		set outLine [split [string trim [viGetLine $viScreenId $i]]]
		set words [expr $words + [llength $outLine]]
	}
	viMsg $viScreenId "$words words"
}
