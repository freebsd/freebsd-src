# word.tcl --
#
# This file defines various procedures for computing word boundaries
# in strings.  This file is primarily needed so Tk text and entry
# widgets behave properly for different platforms.
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# SCCS: @(#) word.tcl 1.2 96/11/20 14:07:22
# 
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# The following variables are used to determine which characters are
# interpreted as white space.  

if {$tcl_platform(platform) == "windows"} {
    # Windows style - any but space, tab, or newline
    set tcl_wordchars "\[^ \t\n\]"
    set tcl_nonwordchars "\[ \t\n\]"
} else {
    # Motif style - any number, letter, or underscore
    set tcl_wordchars {[a-zA-Z0-9_]}
    set tcl_nonwordchars {[^a-zA-Z0-9_]}
}

# tcl_wordBreakAfter --
#
# This procedure returns the index of the first word boundary
# after the starting point in the given string, or -1 if there
# are no more boundaries in the given string.  The index returned refers
# to the first character of the pair that comprises a boundary.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

proc tcl_wordBreakAfter {str start} {
    global tcl_nonwordchars tcl_wordchars
    set str [string range $str $start end]
    if [regexp -indices "$tcl_wordchars$tcl_nonwordchars|$tcl_nonwordchars$tcl_wordchars" $str result] {
	return [expr [lindex $result 1] + $start]
    }
    return -1
}

# tcl_wordBreakBefore --
#
# This procedure returns the index of the first word boundary
# before the starting point in the given string, or -1 if there
# are no more boundaries in the given string.  The index returned
# refers to the second character of the pair that comprises a boundary.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

proc tcl_wordBreakBefore {str start} {
    global tcl_nonwordchars tcl_wordchars
    if {[string compare $start end] == 0} {
	set start [string length $str]
    }
    if [regexp -indices "^.*($tcl_wordchars$tcl_nonwordchars|$tcl_nonwordchars$tcl_wordchars)" [string range $str 0 $start] result] {
	return [lindex $result 1]
    }
    return -1
}

# tcl_endOfWord --
#
# This procedure returns the index of the first end-of-word location
# after a starting index in the given string.  An end-of-word location
# is defined to be the first whitespace character following the first
# non-whitespace character after the starting point.  Returns -1 if
# there are no more words after the starting point.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

proc tcl_endOfWord {str start} {
    global tcl_nonwordchars tcl_wordchars
    if [regexp -indices "$tcl_nonwordchars*$tcl_wordchars+$tcl_nonwordchars" \
	    [string range $str $start end] result] {
	return [expr [lindex $result 1] + $start]
    }
    return -1
}

# tcl_startOfNextWord --
#
# This procedure returns the index of the first start-of-word location
# after a starting index in the given string.  A start-of-word
# location is defined to be a non-whitespace character following a
# whitespace character.  Returns -1 if there are no more start-of-word
# locations after the starting point.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

proc tcl_startOfNextWord {str start} {
    global tcl_nonwordchars tcl_wordchars
    if [regexp -indices "$tcl_wordchars*$tcl_nonwordchars+$tcl_wordchars" \
	    [string range $str $start end] result] {
	return [expr [lindex $result 1] + $start]
    }
    return -1
}

# tcl_startOfPreviousWord --
#
# This procedure returns the index of the first start-of-word location
# before a starting index in the given string.
#
# Arguments:
# str -		String to search.
# start -	Index into string specifying starting point.

proc tcl_startOfPreviousWord {str start} {
    global tcl_nonwordchars tcl_wordchars
    if {[string compare $start end] == 0} {
	set start [string length $str]
    }
    if [regexp -indices \
	    "$tcl_nonwordchars*($tcl_wordchars+)$tcl_nonwordchars*\$" \
	    [string range $str 0 [expr $start - 1]] result word] {
	return [lindex $word 0]
    }
    return -1
}
