# init.tcl --
#
# Default system startup file for Tcl-based applications.  Defines
# "unknown" procedure and auto-load facilities.
#
# SCCS: @(#) init.tcl 1.79 97/06/24 17:18:54
#
# Copyright (c) 1991-1993 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

if {[info commands package] == ""} {
    error "version mismatch: library\nscripts expect Tcl version 7.5b1 or later but the loaded version is\nonly [info patchlevel]"
}
package require -exact Tcl 8.0

# Compute the auto path to use in this interpreter.

if [catch {set auto_path $env(TCLLIBPATH)}] {
    set auto_path ""
}
if {[lsearch -exact $auto_path [info library]] < 0} {
    lappend auto_path [info library]
}
catch {
    foreach dir $tcl_pkgPath {
	if {[lsearch -exact $auto_path $dir] < 0} {
	    lappend auto_path $dir
	}
    }
    unset dir
}

# Conditionalize for presence of exec.

package unknown tclPkgUnknown
if {[info commands exec] == ""} {

    # Some machines, such as the Macintosh, do not have exec. Also, on all
    # platforms, safe interpreters do not have exec.

    set auto_noexec 1
}
set errorCode ""
set errorInfo ""


# unknown --
# This procedure is called when a Tcl command is invoked that doesn't
# exist in the interpreter.  It takes the following steps to make the
# command available:
#
#	1. See if the autoload facility can locate the command in a
#	   Tcl script file.  If so, load it and execute it.
#	2. If the command was invoked interactively at top-level:
#	    (a) see if the command exists as an executable UNIX program.
#		If so, "exec" the command.
#	    (b) see if the command requests csh-like history substitution
#		in one of the common forms !!, !<number>, or ^old^new.  If
#		so, emulate csh's history substitution.
#	    (c) see if the command is a unique abbreviation for another
#		command.  If so, invoke the command.
#
# Arguments:
# args -	A list whose elements are the words of the original
#		command, including the command name.

proc unknown args {
    global auto_noexec auto_noload env unknown_pending tcl_interactive
    global errorCode errorInfo

    # Save the values of errorCode and errorInfo variables, since they
    # may get modified if caught errors occur below.  The variables will
    # be restored just before re-executing the missing command.

    set savedErrorCode $errorCode
    set savedErrorInfo $errorInfo
    set name [lindex $args 0]
    if ![info exists auto_noload] {
	#
	# Make sure we're not trying to load the same proc twice.
	#
	if [info exists unknown_pending($name)] {
	    return -code error "self-referential recursion in \"unknown\" for command \"$name\"";
	}
	set unknown_pending($name) pending;
	set ret [catch {auto_load $name} msg]
	unset unknown_pending($name);
	if {$ret != 0} {
	    return -code $ret -errorcode $errorCode \
		"error while autoloading \"$name\": $msg"
	}
	if ![array size unknown_pending] {
	    unset unknown_pending
	}
	if $msg {
	    set errorCode $savedErrorCode
	    set errorInfo $savedErrorInfo
	    set code [catch {uplevel 1 $args} msg]
	    if {$code ==  1} {
		#
		# Strip the last five lines off the error stack (they're
		# from the "uplevel" command).
		#

		set new [split $errorInfo \n]
		set new [join [lrange $new 0 [expr [llength $new] - 6]] \n]
		return -code error -errorcode $errorCode \
			-errorinfo $new $msg
	    } else {
		return -code $code $msg
	    }
	}
    }
    if {([info level] == 1) && ([info script] == "") \
	    && [info exists tcl_interactive] && $tcl_interactive} {
	if ![info exists auto_noexec] {
	    set new [auto_execok $name]
	    if {$new != ""} {
		set errorCode $savedErrorCode
		set errorInfo $savedErrorInfo
		set redir ""
		if {[info commands console] == ""} {
		    set redir ">&@stdout <@stdin"
		}
		return [uplevel exec $redir $new [lrange $args 1 end]]
	    }
	}
	set errorCode $savedErrorCode
	set errorInfo $savedErrorInfo
	if {$name == "!!"} {
#	    return [uplevel {history redo}]
	    return -code error "!! is disabled until history is fixed in Tcl8.0"
	}
	if [regexp {^!(.+)$} $name dummy event] {
	    return [uplevel [list history redo $event]]
	}
	if [regexp {^\^([^^]*)\^([^^]*)\^?$} $name dummy old new] {
	    return [uplevel [list history substitute $old $new]]
	}

	set ret [catch {set cmds [info commands $name*]} msg]
	if {[string compare $name "::"] == 0} {
	    set name ""
	}
	if {$ret != 0} {
	    return -code $ret -errorcode $errorCode \
		"error in unknown while checking if \"$name\" is a unique command abbreviation: $msg"
	}
	if {[llength $cmds] == 1} {
	    return [uplevel [lreplace $args 0 0 $cmds]]
	}
	if {[llength $cmds] != 0} {
	    if {$name == ""} {
		return -code error "empty command name \"\""
	    } else {
		return -code error \
			"ambiguous command name \"$name\": [lsort $cmds]"
	    }
	}
    }
    return -code error "invalid command name \"$name\""
}

# auto_load --
# Checks a collection of library directories to see if a procedure
# is defined in one of them.  If so, it sources the appropriate
# library file to create the procedure.  Returns 1 if it successfully
# loaded the procedure, 0 otherwise.
#
# Arguments: 
# cmd -			Name of the command to find and load.

proc auto_load cmd {
    global auto_index auto_oldpath auto_path env errorInfo errorCode

    if [info exists auto_index($cmd)] {
	uplevel #0 $auto_index($cmd)
	return [expr {[info commands $cmd] != ""}]
    }
    if ![info exists auto_path] {
	return 0
    }
    if [info exists auto_oldpath] {
	if {$auto_oldpath == $auto_path} {
	    return 0
	}
    }
    set auto_oldpath $auto_path

    # Check if we are a safe interpreter. In that case, we support only
    # newer format tclIndex files.

    set issafe [interp issafe]
    for {set i [expr [llength $auto_path] - 1]} {$i >= 0} {incr i -1} {
	set dir [lindex $auto_path $i]
	set f ""
	if {$issafe} {
	    catch {source [file join $dir tclIndex]}
	} elseif [catch {set f [open [file join $dir tclIndex]]}] {
	    continue
	} else {
	    set error [catch {
		set id [gets $f]
		if {$id == "# Tcl autoload index file, version 2.0"} {
		    eval [read $f]
		} elseif {$id == \
		    "# Tcl autoload index file: each line identifies a Tcl"} {
		    while {[gets $f line] >= 0} {
			if {([string index $line 0] == "#")
				|| ([llength $line] != 2)} {
			    continue
			}
			set name [lindex $line 0]
			set auto_index($name) \
			    "source [file join $dir [lindex $line 1]]"
		    }
		} else {
		    error \
		      "[file join $dir tclIndex] isn't a proper Tcl index file"
		}
	    } msg]
	    if {$f != ""} {
		close $f
	    }
	    if $error {
		error $msg $errorInfo $errorCode
	    }
	}
    }
    if [info exists auto_index($cmd)] {
	uplevel #0 $auto_index($cmd)
	if {[info commands $cmd] != ""} {
	    return 1
	}
    }
    return 0
}

if {[string compare $tcl_platform(platform) windows] == 0} {

# auto_execok --
#
# Returns string that indicates name of program to execute if 
# name corresponds to a shell builtin or an executable in the
# Windows search path, or "" otherwise.  Builds an associative 
# array auto_execs that caches information about previous checks, 
# for speed.
#
# Arguments: 
# name -			Name of a command.

# Windows version.
#
# Note that info executable doesn't work under Windows, so we have to
# look for files with .exe, .com, or .bat extensions.  Also, the path
# may be in the Path or PATH environment variables, and path
# components are separated with semicolons, not colons as under Unix.
#
proc auto_execok name {
    global auto_execs env tcl_platform

    if [info exists auto_execs($name)] {
	return $auto_execs($name)
    }
    set auto_execs($name) ""

    if {[lsearch -exact {cls copy date del erase dir echo mkdir md rename 
	    ren rmdir rd time type ver vol} $name] != -1} {
	return [set auto_execs($name) [list $env(COMSPEC) /c $name]]
    }

    if {[llength [file split $name]] != 1} {
	foreach ext {{} .com .exe .bat} {
	    set file ${name}${ext}
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [set auto_execs($name) [list $file]]
	    }
	}
	return ""
    }

    set path "[file dirname [info nameof]];.;"
    if {[info exists env(WINDIR)]} {
	set windir $env(WINDIR) 
    }
    if {[info exists windir]} {
	if {$tcl_platform(os) == "Windows NT"} {
	    append path "$windir/system32;"
	}
	append path "$windir/system;$windir;"
    }

    if {[info exists env(PATH)]} {
	append path $env(PATH)
    }

    foreach dir [split $path {;}] {
	if {$dir == ""} {
	    set dir .
	}
	foreach ext {{} .com .exe .bat} {
	    set file [file join $dir ${name}${ext}]
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [set auto_execs($name) [list $file]]
	    }
	}
    }
    return ""
}

} else {

# auto_execok --
#
# Returns string that indicates name of program to execute if 
# name corresponds to an executable in the path. Builds an associative 
# array auto_execs that caches information about previous checks, 
# for speed.
#
# Arguments: 
# name -			Name of a command.

# Unix version.
#
proc auto_execok name {
    global auto_execs env

    if [info exists auto_execs($name)] {
	return $auto_execs($name)
    }
    set auto_execs($name) ""
    if {[llength [file split $name]] != 1} {
	if {[file executable $name] && ![file isdirectory $name]} {
	    set auto_execs($name) [list $name]
	}
	return $auto_execs($name)
    }
    foreach dir [split $env(PATH) :] {
	if {$dir == ""} {
	    set dir .
	}
	set file [file join $dir $name]
	if {[file executable $file] && ![file isdirectory $file]} {
	    set auto_execs($name) [list $file]
	    return $auto_execs($name)
	}
    }
    return ""
}

}
# auto_reset --
# Destroy all cached information for auto-loading and auto-execution,
# so that the information gets recomputed the next time it's needed.
# Also delete any procedures that are listed in the auto-load index
# except those defined in this file.
#
# Arguments: 
# None.

proc auto_reset {} {
    global auto_execs auto_index auto_oldpath
    foreach p [info procs] {
	if {[info exists auto_index($p)] && ![string match auto_* $p]
		&& ([lsearch -exact {unknown pkg_mkIndex tclPkgSetup
			tclPkgUnknown} $p] < 0)} {
	    rename $p {}
	}
    }
    catch {unset auto_execs}
    catch {unset auto_index}
    catch {unset auto_oldpath}
}

# auto_mkindex --
# Regenerate a tclIndex file from Tcl source files.  Takes as argument
# the name of the directory in which the tclIndex file is to be placed,
# followed by any number of glob patterns to use in that directory to
# locate all of the relevant files.
#
# Arguments: 
# dir -			Name of the directory in which to create an index.
# args -		Any number of additional arguments giving the
#			names of files within dir.  If no additional
#			are given auto_mkindex will look for *.tcl.

proc auto_mkindex {dir args} {
    global errorCode errorInfo
    set oldDir [pwd]
    cd $dir
    set dir [pwd]
    append index "# Tcl autoload index file, version 2.0\n"
    append index "# This file is generated by the \"auto_mkindex\" command\n"
    append index "# and sourced to set up indexing information for one or\n"
    append index "# more commands.  Typically each line is a command that\n"
    append index "# sets an element in the auto_index array, where the\n"
    append index "# element name is the name of a command and the value is\n"
    append index "# a script that loads the command.\n\n"
    if {$args == ""} {
	set args *.tcl
    }
    foreach file [eval glob $args] {
	set f ""
	set error [catch {
	    set f [open $file]
	    while {[gets $f line] >= 0} {
		if [regexp {^proc[ 	]+([^ 	]*)} $line match procName] {
		    append index "set [list auto_index($procName)]"
		    append index " \[list source \[file join \$dir [list $file]\]\]\n"
		}
	    }
	    close $f
	} msg]
	if $error {
	    set code $errorCode
	    set info $errorInfo
	    catch {close $f}
	    cd $oldDir
	    error $msg $info $code
	}
    }
    set f ""
    set error [catch {
	set f [open tclIndex w]
	puts $f $index nonewline
	close $f
	cd $oldDir
    } msg]
    if $error {
	set code $errorCode
	set info $errorInfo
	catch {close $f}
	cd $oldDir
	error $msg $info $code
    }
}

# pkg_mkIndex --
# This procedure creates a package index in a given directory.  The
# package index consists of a "pkgIndex.tcl" file whose contents are
# a Tcl script that sets up package information with "package require"
# commands.  The commands describe all of the packages defined by the
# files given as arguments.
#
# Arguments:
# dir -			Name of the directory in which to create the index.
# args -		Any number of additional arguments, each giving
#			a glob pattern that matches the names of one or
#			more shared libraries or Tcl script files in
#			dir.

proc pkg_mkIndex {dir args} {
    global errorCode errorInfo
    append index "# Tcl package index file, version 1.0\n"
    append index "# This file is generated by the \"pkg_mkIndex\" command\n"
    append index "# and sourced either when an application starts up or\n"
    append index "# by a \"package unknown\" script.  It invokes the\n"
    append index "# \"package ifneeded\" command to set up package-related\n"
    append index "# information so that packages will be loaded automatically\n"
    append index "# in response to \"package require\" commands.  When this\n"
    append index "# script is sourced, the variable \$dir must contain the\n"
    append index "# full path name of this file's directory.\n"
    set oldDir [pwd]
    cd $dir
    foreach file [eval glob $args] {
	# For each file, figure out what commands and packages it provides.
	# To do this, create a child interpreter, load the file into the
	# interpreter, and get a list of the new commands and packages
	# that are defined.  Define an empty "package unknown" script so
	# that there are no recursive package inclusions.

	set c [interp create]

	# If Tk is loaded in the parent interpreter, load it into the
	# child also, in case the extension depends on it.

	foreach pkg [info loaded] {
	    if {[lindex $pkg 1] == "Tk"} {
		$c eval {set argv {-geometry +0+0}}
		load [lindex $pkg 0] Tk $c
		break
	    }
	}
	$c eval [list set file $file]
	if [catch {
	    $c eval {
		proc dummy args {}
		package unknown dummy
		set origCmds [info commands]
		set dir ""		;# in case file is pkgIndex.tcl
		set pkgs ""

		# Try to load the file if it has the shared library extension,
		# otherwise source it.  It's important not to try to load
		# files that aren't shared libraries, because on some systems
		# (like SunOS) the loader will abort the whole application
		# when it gets an error.

		if {[string compare [file extension $file] \
			[info sharedlibextension]] == 0} {

		    # The "file join ." command below is necessary.  Without
		    # it, if the file name has no \'s and we're on UNIX, the
		    # load command will invoke the LD_LIBRARY_PATH search
		    # mechanism, which could cause the wrong file to be used.

		    load [file join . $file]
		    set type load
		} else {
		    source $file
		    set type source
		}
		foreach i [info commands] {
		    set cmds($i) 1
		}
		foreach i $origCmds {
		    catch {unset cmds($i)}
		}
		foreach i [package names] {
		    if {([string compare [package provide $i] ""] != 0)
			    && ([string compare $i Tcl] != 0)
			    && ([string compare $i Tk] != 0)} {
			lappend pkgs [list $i [package provide $i]]
		    }
		}
	    }
	} msg] {
	    puts "error while loading or sourcing $file: $msg"
	}
	foreach pkg [$c eval set pkgs] {
	    lappend files($pkg) [list $file [$c eval set type] \
		    [lsort [$c eval array names cmds]]]
	}
	interp delete $c
    }
    foreach pkg [lsort [array names files]] {
	append index "\npackage ifneeded $pkg\
		\[list tclPkgSetup \$dir [lrange $pkg 0 0] [lrange $pkg 1 1]\
		[list $files($pkg)]\]"
    }
    set f [open pkgIndex.tcl w]
    puts $f $index
    close $f
    cd $oldDir
}

# tclPkgSetup --
# This is a utility procedure use by pkgIndex.tcl files.  It is invoked
# as part of a "package ifneeded" script.  It calls "package provide"
# to indicate that a package is available, then sets entries in the
# auto_index array so that the package's files will be auto-loaded when
# the commands are used.
#
# Arguments:
# dir -			Directory containing all the files for this package.
# pkg -			Name of the package (no version number).
# version -		Version number for the package, such as 2.1.3.
# files -		List of files that constitute the package.  Each
#			element is a sub-list with three elements.  The first
#			is the name of a file relative to $dir, the second is
#			"load" or "source", indicating whether the file is a
#			loadable binary or a script to source, and the third
#			is a list of commands defined by this file.

proc tclPkgSetup {dir pkg version files} {
    global auto_index

    package provide $pkg $version
    foreach fileInfo $files {
	set f [lindex $fileInfo 0]
	set type [lindex $fileInfo 1]
	foreach cmd [lindex $fileInfo 2] {
	    if {$type == "load"} {
		set auto_index($cmd) [list load [file join $dir $f] $pkg]
	    } else {
		set auto_index($cmd) [list source [file join $dir $f]]
	    } 
	}
    }
}

# tclMacPkgSearch --
# The procedure is used on the Macintosh to search a given directory for files
# with a TEXT resource named "pkgIndex".  If it exists it is sourced in to the
# interpreter to setup the package database.

proc tclMacPkgSearch {dir} {
    foreach x [glob -nocomplain [file join $dir *.shlb]] {
	if [file isfile $x] {
	    set res [resource open $x]
	    foreach y [resource list TEXT $res] {
		if {$y == "pkgIndex"} {source -rsrc pkgIndex}
	    }
	    resource close $res
	}
    }
}

# tclPkgUnknown --
# This procedure provides the default for the "package unknown" function.
# It is invoked when a package that's needed can't be found.  It scans
# the auto_path directories and their immediate children looking for
# pkgIndex.tcl files and sources any such files that are found to setup
# the package database.  (On the Macintosh we also search for pkgIndex
# TEXT resources in all files.)
#
# Arguments:
# name -		Name of desired package.  Not used.
# version -		Version of desired package.  Not used.
# exact -		Either "-exact" or omitted.  Not used.

proc tclPkgUnknown {name version {exact {}}} {
    global auto_path tcl_platform env dir

    if ![info exists auto_path] {
	return
    }
    if {[info exists dir]} {
	set save_dir $dir
    }
    for {set i [expr [llength $auto_path] - 1]} {$i >= 0} {incr i -1} {
	foreach file [glob -nocomplain [file join [lindex $auto_path $i] \
		* pkgIndex.tcl]] {
	    set dir [file dirname $file]
	    if [catch {source $file} msg] {
		puts stderr \
		    "error reading package index file $file: $msg"
	    }
	}
	set dir [lindex $auto_path $i]
	set file [file join $dir pkgIndex.tcl]
	if [file readable $file] {
	    if [catch {source $file} msg] {
		puts stderr \
		    "error reading package index file $file: $msg"
	    }
	}
	# On the Macintosh we also look in the resource fork 
	# of shared libraries
	if {$tcl_platform(platform) == "macintosh"} {
	    set dir [lindex $auto_path $i]
	    tclMacPkgSearch $dir
	    foreach x [glob -nocomplain [file join $dir *]] {
		if [file isdirectory $x] {
		    set dir $x
		    tclMacPkgSearch $dir
		}
	    }
	}
    }
    if {[info exists save_dir]} {
	set dir $save_dir
    } else {
	unset dir
    }
}
